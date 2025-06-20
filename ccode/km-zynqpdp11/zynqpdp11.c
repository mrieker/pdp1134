//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// kernel module for raspi to map the zynq.vhd fpga 4K page
// sets it up as a mmap page accessed from /proc/zynqpdp11

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/io-mapping.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

#include "zgintdefs.h"

#define ZG_PROCNAME "zynqpdp11" // name in /proc/
#define ZG_PHYSADDR 0x43C00000  // physical address of fpga/arm page
#define ZG_INTVEC 31            // interrupt request line used by zynq.v -> arm

typedef struct FoCtx {
    unsigned long pagepa;
} FoCtx;

typedef struct ISRWait {
    struct ISRWait *next;
    struct ISRWait **prev;
    struct task_struct *proc;
    uint32_t flags;
} ISRWait;


static DEFINE_SPINLOCK (isrlock);
static int myirqno;
static ISRWait *isrwaits;
static struct io_mapping *pageknlmapping;
static uint32_t volatile *pageknladdress;

static int zg_open (struct inode *inode, struct file *filp);
static ssize_t zg_read (struct file *filp, char __user *buff, size_t size, loff_t *posn);
static ssize_t zg_write (struct file *filp, char const __user *buff, size_t size, loff_t *posn);
static int zg_release (struct inode *inode, struct file *filp);
static long zg_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static int zg_mmap (struct file *filp, struct vm_area_struct *vma);
static irqreturn_t zg_isr (int irq, void *arg);


static struct file_operations const zg_fops = {
    .mmap    = zg_mmap,
    .open    = zg_open,
    .read    = zg_read,
    .release = zg_release,
    .unlocked_ioctl = zg_ioctl,
    .write   = zg_write
};

int init_module ()
{
    int i, irqno, j, rc;
    int irqcts[3] = { 0, 0, 0 };
    int irqnos[3] = { 0, 0, 0 };
    unsigned long probedirqs;

    struct proc_dir_entry *procDirEntry = proc_create (ZG_PROCNAME, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH, NULL, &zg_fops);
    if (procDirEntry == NULL) {
        printk ("zynqgpio: error creating /proc/%s entry\n", ZG_PROCNAME);
        return -1;
    }

    pageknlmapping = io_mapping_create_wc (ZG_PHYSADDR, 4096);
    if (pageknlmapping == NULL) {
        printk ("zynqgpio: io_mapping_create_wc %08X failed\n", ZG_PHYSADDR);
        cleanup_module ();
        return -ENOMEM;
    }
    pageknladdress = io_mapping_map_wc (pageknlmapping, 0, 4096);
    if (pageknladdress == NULL) {
        printk ("zynqgpio: io_mapping_map_wc %08X failed\n", ZG_PHYSADDR);
        cleanup_module ();
        return -ENOMEM;
    }

    printk ("zynqgpio: probing irqs\n");
    for (i = 0; i < 5; i ++) {
        probedirqs = probe_irq_on ();
        pageknladdress[ZG_INTENABS] = ZGINT_ARM;
        pageknladdress[ZG_INTFLAGS] = ZGINT_ARM;
        irqno = probe_irq_off (probedirqs);     // interrupt should be instant
        pageknladdress[ZG_INTENABS] = 0;
        pageknladdress[ZG_INTFLAGS] = 0;
        if (irqno > 0) {
            printk ("zynqgpio: probing got irq %d\n", irqno);
            for (j = 0; j < 3; j ++) {
                if ((irqnos[j] == irqno) || (irqnos[j] == 0)) {
                    irqnos[j] = irqno;
                    irqcts[j] ++;
                }
            }
        }
    }
    irqno = 0;
    for (j = 0; j < 3; j ++) {
        if (irqcts[j] >= 3) irqno = irqnos[j];
    }
    if (irqno <= 0) {
        printk ("zynqgpio: irq not found\n");
        cleanup_module ();
        return -EBADF;
    }

    rc = request_irq (irqno, zg_isr, 0, "zg", &myirqno);
    if (rc < 0) {
        printk ("zynqgpio: error %d requesting interrupt %d\n", rc, irqno);
        cleanup_module ();
        return rc;
    }
    myirqno = irqno;
    printk ("zynqgpio: registered interrupt %d\n", irqno);

    return 0;
}

void cleanup_module ()
{
    remove_proc_entry (ZG_PROCNAME, NULL);

    if (pageknladdress != NULL) {
        io_mapping_unmap ((void *) pageknladdress);
        pageknladdress = NULL;
    }
    if (pageknlmapping != NULL) {
        io_mapping_free (pageknlmapping);
        pageknlmapping = NULL;
    }

    if (myirqno > 0) {
        free_irq (myirqno, &myirqno);
        myirqno = 0;
    }
}

static int zg_open (struct inode *inode, struct file *filp)
{
    FoCtx *foctx;

    filp->private_data = NULL;
    if (filp->f_flags != O_RDWR) return -EACCES;
    foctx = kmalloc (sizeof *foctx, GFP_KERNEL);
    if (foctx == NULL) return -ENOMEM;
    memset (foctx, 0, sizeof *foctx);
    filp->private_data = foctx;
    return 0;
}

static ssize_t zg_read (struct file *filp, char __user *buff, size_t size, loff_t *posn)
{
    return -EIO;
}

static ssize_t zg_write (struct file *filp, char const __user *buff, size_t size, loff_t *posn)
{
    return -EIO;
}

static int zg_release (struct inode *inode, struct file *filp)
{
    FoCtx *foctx = filp->private_data;
    if (foctx != NULL) {
        filp->private_data = NULL;
        kfree (foctx);
    }
    return 0;
}

static long zg_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    //FoCtx *foctx = filp->private_data;
    switch (cmd) {
        case ZGIOCTL_WFI: {
            unsigned long intena;
            spin_lock_irqsave (&isrlock, intena);
            if (! (pageknladdress[ZG_INTFLAGS] & arg)) {
                ISRWait isrwait;

                isrwait.flags = arg;
                isrwait.proc  = current;
                isrwait.next  = isrwaits;
                isrwait.prev  = &isrwaits;
                if (isrwaits != NULL) isrwaits->prev = &isrwait.next;
                isrwaits = &isrwait;

                pageknladdress[ZG_INTENABS] |= arg;

                current->state = TASK_INTERRUPTIBLE;

                spin_unlock_irqrestore (&isrlock, intena);

                if (! signal_pending (current)) {
                    schedule ();
                }

                spin_lock_irqsave (&isrlock, intena);
                *isrwait.prev = isrwait.next;
                if (isrwait.next != NULL) isrwait.next->prev = isrwait.prev;
                current->state = TASK_RUNNING;
            }
            spin_unlock_irqrestore (&isrlock, intena);
            return 0;
        }
    }
    return -EINVAL;
}

static int zg_mmap (struct file *filp, struct vm_area_struct *vma)
{
    int rc;

    long len = vma->vm_end - vma->vm_start;

    if (vma->vm_pgoff == 0) {
        if (len != PAGE_SIZE) {
            printk ("zg_mmap: not mapping single page at va %lX..%lX => %lX\n", vma->vm_start, vma->vm_end, len);
            return -EINVAL;
        }
        vma->vm_page_prot = pgprot_noncached (vma->vm_page_prot);
        rc = vm_iomap_memory (vma, ZG_PHYSADDR, PAGE_SIZE);
        if (rc < 0) {
            printk ("zg_mmap: iomap %lu status %d\n", vma->vm_pgoff, rc);
        }
        return rc;
    }

    printk ("zg_map: bad file page offset %lu\n", vma->vm_pgoff);
    return -EINVAL;
}

// called at isr level when bits in ZG_INTFLAGS & ZG_INTENABS are set
// wakes any threads waiting for that to happen
static irqreturn_t zg_isr (int irq, void *arg)
{
    irqreturn_t rc = IRQ_NONE;
    ISRWait *isrwait;
    uint32_t enabs = 0;
    uint32_t flags;
    unsigned long intena;

    spin_lock_irqsave (&isrlock, intena);
    pageknladdress[ZG_INTENABS] = 0;            // always shut it off for a few cycles for edge triggering
    flags = pageknladdress[ZG_INTFLAGS];        // get bits currently asserted
    for (isrwait = isrwaits; isrwait != NULL; isrwait = isrwait->next) {
        if (flags & isrwait->flags) {
            wake_up_process (isrwait->proc);    // wake whatever is waiting for any asserted bit
            rc = IRQ_HANDLED;
        } else {
            enabs |= isrwait->flags;            // still waiting for these bits
        }
    }
    pageknladdress[ZG_INTENABS] = enabs;        // re-enable anything still being waited for
    spin_unlock_irqrestore (&isrlock, intena);

    return rc;
}
