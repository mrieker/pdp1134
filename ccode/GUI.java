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

// GUI panel

// run directly on zturn:
//  ./GUI

// apt install default-jdk
// ln -s /usr/lib/jvm/java-11-openjdk-armhf /opt/jdk

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import javax.imageio.ImageIO;
import javax.swing.BoxLayout;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.Timer;

public class GUI extends JPanel {

    public final static int UPDMS = 3;

    public final static Dimension buttondim = new Dimension (117, 117);
    public final static ImageIcon buttonin  = new ImageIcon (GUI.class.getClassLoader ().getResource ("violetcirc117.png"));
    public final static ImageIcon buttonout = new ImageIcon (GUI.class.getClassLoader ().getResource ("purplecirc117.png"));

    public final static Dimension leddim = new Dimension (52, 52);
    public final static ImageIcon ledon  = new ImageIcon (GUI.class.getClassLoader ().getResource ("violetcirc52.png"));
    public final static ImageIcon ledoff = new ImageIcon (GUI.class.getClassLoader ().getResource ("purplecirc52.png"));

    public static BufferedImage redeyeim;

    public static void main (String[] args)
            throws Exception
    {
        if (args.length > 0) {
            System.err.println ("unknown argument/option " + args[0]);
            System.exit (1);
        }

        redeyeim = ImageIO.read (GUI.class.getResourceAsStream ("redeyeclip36.png"));

        // open access to Zynq board
        GUIZynqPage.open ();

        // create window and show it
        JFrame jframe = new JFrame ("PDP-11/34A");
        jframe.setDefaultCloseOperation (JFrame.EXIT_ON_CLOSE);
        jframe.setContentPane (new GUI ());
        SwingUtilities.invokeLater (new Runnable () {
            @Override
            public void run ()
            {
                jframe.pack ();
                jframe.setLocationRelativeTo (null);
                jframe.setVisible (true);

                // get initial switch register contents from 777570
                int srint = GUIZynqPage.rdmem (0777570);
                write16switches (srint);

                // loop timer to update display from fpga & buttons/switches
                Timer runupdatimer = new Timer (UPDMS, updisplay);
                runupdatimer.start ();
            }
        });
    }

    ////////////////////
    //  GUI ELEMENTS  //
    ////////////////////

    public static JLabel[] addrlbls = new JLabel[18];
    public static LED[]    addrleds = new LED[18];
    public static LED[]    dataleds = new LED[16];
    public static LED[]    lregleds = new LED[16];
    public static Switch[] switches = new Switch[18];
    public static LED berrled;
    public static LED runled;

    public static LdAdButton  ldadbutton;
    public static ExamButton  exambutton;
    public static DepButton   depbutton;
    public static HaltButton  haltbutton;
    public static StepButton  stepbutton;
    public static ContButton  contbutton;
    public static StartButton startbutton;
    public static ResetButton resetbutton;
    public static BootButton  bootbutton;

    public static JLabel messagelabel;

    // update display with processor state - runs continuously
    // if processor is running, display current processor state
    // otherwise, leave display alone so ldad/exam/dep buttons will work
    public final static ActionListener updisplay =
        new ActionListener () {
            @Override
            public void actionPerformed (ActionEvent ae)
            {
                // read values from zynq fpga
                int sample_addr = GUIZynqPage.addr ();
                int sample_data = GUIZynqPage.data ();
                int sample_lreg = GUIZynqPage.getlr ();
                int sample_sreg = GUIZynqPage.getsr ();
                int sample_running = GUIZynqPage.running ();

                // run light always says what is happening
                runled.setOn (sample_running > 0);

                // light register lights also always reflect the fpga register
                writelregleds (sample_lreg);

                // same with switch register - in case z11ctrl or similar flips them
                // the upper 2 switches are only kept here (no way for z11ctrl etc to flip them)
                write16switches (sample_sreg & 0177777);

                // update grayed buttons based on running state
                //  running = +1 : processor is running
                //             0 : processor halted but is resumable
                //            -1 : processor halted, reset required
                if (lastrunning != sample_running) {
                    ldadbutton.setEnabled  (sample_running <= 0);
                    exambutton.setEnabled  (sample_running <= 0);
                    depbutton.setEnabled   (sample_running <= 0);
                    haltbutton.setEnabled  (sample_running  > 0);
                    stepbutton.setEnabled  (sample_running == 0);
                    contbutton.setEnabled  (sample_running == 0);
                    startbutton.setEnabled (sample_running <= 0);
                    resetbutton.setEnabled (true);
                    bootbutton.setEnabled  (true);
                }

                // if processor currently running, update lights from what fpga last captured from unibus
                if (sample_running > 0) {
                    writeaddrleds (sample_addr);
                    writedataleds (sample_data);
                    berrled.setOn (false);
                }

                // if processor just halted, display PC,PS
                else if (lastrunning > 0) {
                    int pc = GUIZynqPage.rdmem (0777707);
                    int ps = GUIZynqPage.rdmem (0777776);
                    String text = "";
                    if (pc >= 0) {
                        text += String.format ("stopped at PC %06o", pc);
                        int pcpa = vatopa (pc, ps >> 14);
                        berrled.setOn (pcpa < 0);
                        if (pcpa < 0) {
                            text += ", " + vatopaerr[~pcpa];
                        } else if (pcpa != pc) {
                            text += String.format (" (pa %06o)", pcpa);
                        }
                        loadedaddress = pc;
                        if (ps >= 0) loadedaddress |= (ps & 0140000) << 2;
                        loadedaddrvirt = true;
                        depbutton.autoincrement = false;
                        exambutton.autoincrement = false;
                        writeaddrleds (loadedaddress);
                    } else {
                        writeaddrleds (-1);
                        text += "stopped at PC unknown";
                    }
                    writedataleds (ps);
                    if (ps >= 0) {
                        text += String.format (", PS %06o", ps);
                    } else {
                        text += ", PS unknown";
                    }
                    messagelabel.setText (text);
                }

                if (lastrunning != sample_running) {
                    lastrunning = sample_running;
                    updateaddrlabel ();
                }
            }
        };

    public static int lastrunning = 12345;

    // build the display
    public GUI ()
    {
        setLayout (new BoxLayout (this, BoxLayout.Y_AXIS));

        JPanel ledbox = new JPanel ();
        ledbox.setLayout (new BoxLayout (ledbox, BoxLayout.X_AXIS));
        add (ledbox);

        JPanel bits1715 = new JPanel ();
        JPanel bits1412 = new JPanel ();
        JPanel bits1109 = new JPanel ();
        JPanel bits0806 = new JPanel ();
        JPanel bits0503 = new JPanel ();
        JPanel bits0200 = new JPanel ();

        bits1715.setBackground (Color.MAGENTA);
        bits1412.setBackground (Color.RED);
        bits1109.setBackground (Color.MAGENTA);
        bits0806.setBackground (Color.RED);
        bits0503.setBackground (Color.MAGENTA);
        bits0200.setBackground (Color.RED);

        ledbox.add (new JLabel ("  "));
        ledbox.add (bits1715);
        ledbox.add (new JLabel ("  "));
        ledbox.add (bits1412);
        ledbox.add (new JLabel ("  "));
        ledbox.add (bits1109);
        ledbox.add (new JLabel ("  "));
        ledbox.add (bits0806);
        ledbox.add (new JLabel ("  "));
        ledbox.add (bits0503);
        ledbox.add (new JLabel ("  "));
        ledbox.add (bits0200);
        ledbox.add (new JLabel ("  "));

        bits1715.setLayout (new GridLayout (9, 3));
        bits1412.setLayout (new GridLayout (9, 3));
        bits1109.setLayout (new GridLayout (9, 3));
        bits0806.setLayout (new GridLayout (9, 3));
        bits0503.setLayout (new GridLayout (9, 3));
        bits0200.setLayout (new GridLayout (9, 3));

        // row 0 - address label
        bits1715.add (addrlbls[17] = centeredLabel (""));
        bits1715.add (addrlbls[16] = centeredLabel (""));
        bits1715.add (addrlbls[15] = centeredLabel (""));
        bits1412.add (addrlbls[14] = centeredLabel (""));
        bits1412.add (addrlbls[13] = centeredLabel (""));
        bits1412.add (addrlbls[12] = centeredLabel (""));
        bits1109.add (addrlbls[11] = centeredLabel (""));
        bits1109.add (addrlbls[10] = centeredLabel (""));
        bits1109.add (addrlbls[ 9] = centeredLabel (""));
        bits0806.add (addrlbls[ 8] = centeredLabel (""));
        bits0806.add (addrlbls[ 7] = centeredLabel (""));
        bits0806.add (addrlbls[ 6] = centeredLabel ("A"));
        bits0503.add (addrlbls[ 5] = centeredLabel ("D"));
        bits0503.add (addrlbls[ 4] = centeredLabel ("D"));
        bits0503.add (addrlbls[ 3] = centeredLabel ("R"));
        bits0200.add (addrlbls[ 2] = centeredLabel ("E"));
        bits0200.add (addrlbls[ 1] = centeredLabel ("S"));
        bits0200.add (addrlbls[ 0] = centeredLabel ("S"));

        // row 1 - address bits
        for (int i = 3; -- i >= 0;) {
            bits1715.add (addrleds[15+i] = new LED ());
            bits1412.add (addrleds[12+i] = new LED ());
            bits1109.add (addrleds[ 9+i] = new LED ());
            bits0806.add (addrleds[ 6+i] = new LED ());
            bits0503.add (addrleds[ 3+i] = new LED ());
            bits0200.add (addrleds[ 0+i] = new LED ());
        }

        // row 2 - data label
        bits1715.add (centeredLabel ("RUN"));
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel ("D"));
        bits0200.add (centeredLabel ("A"));
        bits0200.add (centeredLabel ("T"));
        bits0200.add (centeredLabel ("A"));

        // row 3 - data bits
        bits1715.add (runled = new LED ());
        bits1715.add (centeredLabel (""));
        bits1715.add (dataleds[15] = new LED ());
        for (int i = 3; -- i >= 0;) {
            bits1412.add (dataleds[12+i] = new LED ());
            bits1109.add (dataleds[ 9+i] = new LED ());
            bits0806.add (dataleds[ 6+i] = new LED ());
            bits0503.add (dataleds[ 3+i] = new LED ());
            bits0200.add (dataleds[ 0+i] = new LED ());
        }

        // row 4 - 777570 lights label
        bits1715.add (centeredLabel ("BER"));
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0503.add (centeredLabel ("L"));
        bits0503.add (centeredLabel ("I"));
        bits0503.add (centeredLabel ("G"));
        bits0200.add (centeredLabel ("H"));
        bits0200.add (centeredLabel ("T"));
        bits0200.add (centeredLabel ("S"));

        // row 5 - 777570 lights
        bits1715.add (berrled = new FlashingLED ());
        bits1715.add (centeredLabel (""));
        bits1715.add (lregleds[15] = new LED ());
        for (int i = 3; -- i >= 0;) {
            bits1412.add (lregleds[12+i] = new LED ());
            bits1109.add (lregleds[ 9+i] = new LED ());
            bits0806.add (lregleds[ 6+i] = new LED ());
            bits0503.add (lregleds[ 3+i] = new LED ());
            bits0200.add (lregleds[ 0+i] = new LED ());
        }

        // row 6 - switches label
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits1109.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel ("S"));
        bits0806.add (centeredLabel ("W"));
        bits0503.add (centeredLabel ("I"));
        bits0503.add (centeredLabel ("T"));
        bits0503.add (centeredLabel ("C"));
        bits0200.add (centeredLabel ("H"));
        bits0200.add (centeredLabel ("E"));
        bits0200.add (centeredLabel ("S"));

        // row 7 - switch register
        for (int i = 3; -- i >= 0;) {
            bits1715.add (switches[15+i] = new Switch ());
            bits1412.add (switches[12+i] = new Switch ());
            bits1109.add (switches[ 9+i] = new Switch ());
            bits0806.add (switches[ 6+i] = new Switch ());
            bits0503.add (switches[ 3+i] = new Switch ());
            bits0200.add (switches[ 0+i] = new Switch ());
        }

        // buttons along the bottom
        JPanel buttonbox1 = new JPanel ();
        buttonbox1.setLayout (new BoxLayout (buttonbox1, BoxLayout.X_AXIS));
        add (buttonbox1);

        buttonbox1.add (ldadbutton  = new LdAdButton  ());
        buttonbox1.add (exambutton  = new ExamButton  ());
        buttonbox1.add (depbutton   = new DepButton   ());
        buttonbox1.add (haltbutton  = new HaltButton  ());
        buttonbox1.add (stepbutton  = new StepButton  ());
        buttonbox1.add (contbutton  = new ContButton  ());
        buttonbox1.add (startbutton = new StartButton ());
        buttonbox1.add (resetbutton = new ResetButton ());
        buttonbox1.add (bootbutton  = new BootButton  ());

        JPanel messagebox = new JPanel ();
        add (messagebox);
        messagebox.add (messagelabel = new JLabel ());
        messagelabel.setText (" ");
    }

    public static JLabel centeredLabel (String label)
    {
        JLabel jl = new JLabel (label);
        jl.setHorizontalAlignment (JLabel.CENTER);
        return jl;
    }

    // read 18-bit value from SR (switch register) leds
    public static int read18switches ()
    {
        int sr = 0;
        for (int i = 0; i < 18; i ++) {
            Switch sw = switches[i];
            if (sw.ison) sr |= 1 << i;
        }
        return sr;
    }

    // write 16-bit value to SR (switch register) leds
    // leave the upper 2 switches alone
    public static void write16switches (int sr)
    {
        for (int i = 0; i < 16; i ++) {
            Switch sw = switches[i];
            sw.setOn ((sr & (1 << i)) != 0);
        }
    }

    // write 18-bit value to address leds
    public static void writeaddrleds (int addr)
    {
        for (int i = 0; i < 18; i ++) {
            addrleds[i].setOn ((addr & (1 << i)) != 0);
        }
    }

    // write 16-bit value to data leds
    public static void writedataleds (int data)
    {
        for (int i = 0; i < 16; i ++) {
            dataleds[i].setOn ((data & (1 << i)) != 0);
        }
    }

    // write 16-bit value to light register leds
    public static void writelregleds (int lreg)
    {
        for (int i = 0; i < 16; i ++) {
            lregleds[i].setOn ((lreg & (1 << i)) != 0);
        }
    }

    public static class FlashingLED extends LED implements ActionListener {
        public FlashingLED ()
        {
            Timer flasher = new Timer (333, this);
            flasher.start ();
        }

        @Override   // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            loc = (loc == Color.YELLOW) ? Color.RED : Color.YELLOW;
            repaint ();
        }
    }

    public static class LED extends JButton {
        public boolean ison;
        public Color loc;

        private int dotxl, dotyt;

        public LED ()
        {
            this (Color.RED);
        }

        public LED (Color lo)
        {
            loc = lo;
            setMaximumSize (leddim);
            setMinimumSize (leddim);
            setPreferredSize (leddim);
            setSize (leddim);
            setIcon (ledoff);

            dotxl = (int) (leddim.getWidth  () - redeyeim.getWidth  ()) / 2 + 1;
            dotyt = (int) (leddim.getHeight () - redeyeim.getHeight ()) / 2 - 1;
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                setIcon (ison ? ledon : ledoff);
                ////repaint ();
            }
        }

        @Override
        public void paint (Graphics g)
        {
            super.paint (g);
            if (ison) {
                g.drawImage (redeyeim, dotxl, dotyt, null);
            }
        }
    }

    public static class Switch extends LED implements ActionListener {
        public Switch ()
        {
            addActionListener (this);
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            setOn (! ison);
            GUIZynqPage.setsr (read18switches () & 0177777);
        }
    }

    ///////////////////////////////
    // PROCESSOR CONTROL BUTTONS //
    ///////////////////////////////

    public static class StepButton extends MemButton {
        public StepButton ()
        {
            super ("STEP");
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            berrled.setOn (false);
            messagelabel.setText ("stepping processor");
            GUIZynqPage.step ();                     // tell processor to step single instruction
            checkforhalt ();                    // make sure it halted
            lastrunning = 12345;                // force updisplay to refresh everything
            updisplay.actionPerformed (null);   // update display
        }
    }

    // - stop processing instructions
    public static class HaltButton extends MemButton {
        public HaltButton ()
        {
            super ("HALT");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            berrled.setOn (false);
            messagelabel.setText ("halting processor");
            GUIZynqPage.halt ();                     // tell processor to stop executing instructions
            checkforhalt ();                    // make sure it halted
            updisplay.actionPerformed (null);   // update display
        }
    }

    // - continue processing instructions
    public static class ContButton extends MemButton {
        public ContButton ()
        {
            super ("CONT");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            berrled.setOn (false);
            messagelabel.setText ("resuming processor");
            GUIZynqPage.cont ();
            messagelabel.setText ("processor resumed");
            updisplay.actionPerformed (null);
        }
    }

    // - reset I/O devices and processor
    public static class ResetButton extends MemButton {
        public ResetButton ()
        {
            super ("RESET");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            berrled.setOn (false);
            messagelabel.setText ("resetting processor");
            GUIZynqPage.reset ();
            checkforhalt ();
            messagelabel.setText ("processor reset complete");
            updisplay.actionPerformed (null);
        }
    }

    // - boot processor
    public static class BootButton extends MemButton {
        public Process bootprocess;

        public BootButton ()
        {
            super ("BOOT");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            resetbutton.actionPerformed (ae);

            messagelabel.setText ("booting...");
            int switches = read18switches ();
            Thread t = new Thread () {
                @Override
                public void run ()
                {
                    Process oldbp = bootprocess;
                    bootprocess = null;
                    if (oldbp != null) {
                        oldbp.destroyForcibly ();
                    }
                    try {
                        String ccode = System.getProperty ("ccode");
                        ProcessBuilder pb = new ProcessBuilder (ccode + "/guiboot.sh", Integer.toString (switches));
                        pb.redirectErrorStream (true);  // "2>&1"
                        bootprocess = pb.start ();
                        BufferedReader br = new BufferedReader (new InputStreamReader (bootprocess.getInputStream ()));
                        for (String line; (line = br.readLine ()) != null;) {
                            System.out.println ("BootButton: " + line);
                            messageFromThread (line);
                        }
                        bootprocess.waitFor ();
                    } catch (Exception e) {
                        e.printStackTrace ();
                        messageFromThread (e.getMessage ());
                    }
                }
            };
            t.start ();
        }
    }

    public static void messageFromThread (String msg)
    {
        SwingUtilities.invokeLater (new Runnable () {
            @Override
            public void run ()
            {
                messagelabel.setText (msg);
            }
        });
    }

    // - start running program after resetting processor
    public static class StartButton extends MemButton {
        public StartButton ()
        {
            super ("START");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            berrled.setOn (false);
            messagelabel.setText ("resetting processor");
            GUIZynqPage.reset ();
            if (checkforhalt ()) {
                messagelabel.setText ("writing PC and PS");
                if (GUIZynqPage.wrmem (0777707, loadedaddress) < 0) {
                    messagelabel.setText ("writing PC failed");
                } else if (GUIZynqPage.wrmem (0777776, 0340) < 0) {
                    messagelabel.setText ("writing PS failed");
                } else {
                    messagelabel.setText (String.format ("starting at %06o", loadedaddress));
                    GUIZynqPage.cont  ();
                }
            }
            updisplay.actionPerformed (null);
        }
    }

    // halt was requested, wait here for processor to actually halt
    public static boolean checkforhalt ()
    {
        for (int i = 0; GUIZynqPage.running () > 0; i ++) {
            if (i > 100000) {
                messagelabel.setText ("processor failed to halt");
                return false;
            }
        }
        return true;
    }

    ///////////////////////////
    // MEMORY ACCESS BUTTONS //
    ///////////////////////////

    public static boolean loadedaddrvirt;
    public static int loadedaddress;

    // - load address button
    public static class LdAdButton extends MemButton {
        public long pressedat;

        public LdAdButton ()
        {
            super ("LD.AD");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("");
            depbutton.autoincrement = false;
            exambutton.autoincrement = false;
            loadedaddress = read18switches () & 0777777;
            if ((loadedaddress < 0777700) || (loadedaddress > 0777717)) {
                loadedaddress &= 0777776;
            }
            writeaddrleds (loadedaddress);
            writedataleds (0);
            long releasedat = System.currentTimeMillis ();
            loadedaddrvirt = (releasedat - pressedat) >= 800;
            updateaddrlabel ();
            String as = String.format ("loaded %s address %06o", loadedaddrvirt ? "virtual" : "physical", loadedaddress);
            boolean berr = false;
            if (loadedaddrvirt) {
                int pa = vatopa (loadedaddress & 0177777, loadedaddress >> 14);
                if (pa < 0) {
                    as  += ", " + vatopaerr[~pa];
                    berr = true;
                } else {
                    as  += String.format (" (pa %06o)", pa);
                }
            }
            berrled.setOn (berr);
            messagelabel.setText (as);
        }

        @Override   // MouseListener
        public void mousePressed(MouseEvent e)
        {
            super.mousePressed (e);
            pressedat = System.currentTimeMillis ();
        }
    }

    // update VIRT/PHYS tag on address lights label
    public static void updateaddrlabel ()
    {
        if (loadedaddrvirt & (GUIZynqPage.running () <= 0)) {
            addrlbls[15].setText ("");
            addrlbls[14].setText ("V");
            addrlbls[13].setText ("I");
            addrlbls[12].setText ("R");
            addrlbls[11].setText ("T");
            addrlbls[10].setText ("U");
            addrlbls[ 9].setText ("A");
            addrlbls[ 8].setText ("L");
        } else {
            addrlbls[15].setText ("P");
            addrlbls[14].setText ("H");
            addrlbls[13].setText ("Y");
            addrlbls[12].setText ("S");
            addrlbls[11].setText ("I");
            addrlbls[10].setText ("C");
            addrlbls[ 9].setText ("A");
            addrlbls[ 8].setText ("L");
        }
    }

    // - examine button
    public static class ExamButton extends ExDepButton {
        public ExamButton ()
        {
            super ("EXAM");
        }

        @Override  // ExDepButton
        public String memop ()
        {
            return "examined";
        }

        @Override  // ExDepButton
        public int rwmem (int pa)
        {
            return GUIZynqPage.rdmem (pa);
        }
    }

    // - deposit button
    public static class DepButton extends ExDepButton {
        public DepButton ()
        {
            super ("DEP");
        }

        @Override  // ExDepButton
        public String memop ()
        {
            return "deposited to";
        }

        @Override  // ExDepButton
        public int rwmem (int pa)
        {
            int data = read18switches () & 0177777;
            return GUIZynqPage.wrmem (pa, data);
        }
    }

    // - examine/deposit button
    public static abstract class ExDepButton extends MemButton {
        public boolean autoincrement;

        public ExDepButton (String lbl)
        {
            super (lbl);
        }

        public abstract String memop ();
        public abstract int rwmem (int pa);

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("");

            // get physical address
            int pa = loadedaddrvirt ? vatopa (loadedaddress & 0177777, loadedaddress >> 16) : loadedaddress;

            // possibly auto-increment the address
            if (autoincrement && (pa >= 0)) {
                int inc = ((pa >= 0777700) && (pa <= 0777717)) ? 1 : 2;
                if (loadedaddrvirt) {
                    loadedaddress = (loadedaddress & 0600000) | ((loadedaddress + inc) & 0177777);
                    pa = vatopa (loadedaddress & 0177777, loadedaddress >> 16);
                } else {
                    loadedaddress = (loadedaddress + inc) & 0777777;
                    pa = loadedaddress;
                }
            }

            // display the possibly updated address (virtual or physical as the user originally selected)
            writeaddrleds (loadedaddress);
            String st = String.format ("%s %s address %06o", memop (), loadedaddrvirt ? "virtual" : "physical", loadedaddress);;

            int rc;
            if (pa < 0) {
                st += String.format (", %s", vatopaerr[~pa]);
                rc  = pa;
            } else {
                if (loadedaddrvirt) {
                    st += String.format (" (pa %06o)", pa);
                }

                // read or write memory location
                rc = rwmem (pa);
                if (rc < 0) {
                    st += ", " + rwmemerr[~rc];
                } else {
                    st += String.format (", data %06o", rc);
                }
            }

            berrled.setOn (rc < 0);
            writedataleds (rc & 0177777);

            messagelabel.setText (st);

            depbutton.autoincrement  = false;
            exambutton.autoincrement = false;
            autoincrement = rc >= 0;
        }
    }

    // convert virtual address to physical address
    public static int vatopa (int va, int mode)
    {
        if ((va < 0) || (va > 0177777)) return -1;
        int mmr0 = GUIZynqPage.rdmem (0777572);
        if (mmr0 < 0) return -2;
        if ((mmr0 & 1) == 0) {
            if (va >= 0160000) va |= 0760000;
            return va;
        }
        if (mode < 0) {
            int psw = GUIZynqPage.rdmem (0777776);
            if (psw < 0) return -3;
            mode = psw >> 14;
        }
        int regs = 0;
        switch (mode) {
            case 0: regs = 0772300; break;
            case 3: regs = 0777600; break;
            default: return -4;
        }
        int page = va >> 13;
        int pdr = GUIZynqPage.rdmem (regs + 2 * page);
        if (pdr < 0) return -5;
        if ((pdr & 2) == 0) return -6;
        int blok = (va  >> 6) & 0177;
        int len  = (pdr >> 8) & 0177;
        if ((pdr & 8) != 0) {
            if (blok < len) return -7;
        } else {
            if (blok > len) return -8;
        }
        int par = GUIZynqPage.rdmem (regs + 040 + 2 * page);
        if (par < 0) return -9;
        return (va & 017777) + ((par & 07777) << 6);
    }

    public static String[] vatopaerr = {
        "va out of range",
        "unable to read mmr0",
        "unable to read psw",
        "invalid processor mode",
        "unable to read pdr",
        "page marked no-access",
        "below length of expand-down page",
        "above length of expand-up page",
        "unable to read par" };

    public static String[] rwmemerr = {
        "bus timed out",
        "parity error" };

    public static abstract class MemButton extends JButton implements ActionListener, MouseListener {
        public MemButton (String lbl)
        {
            super (lbl);

            addActionListener (this);

            setMaximumSize (buttondim);
            setMinimumSize (buttondim);
            setPreferredSize (buttondim);
            setSize (buttondim);
            setForeground (Color.WHITE);
            setIcon (buttonout);
            addMouseListener (this);
            setVerticalTextPosition (SwingConstants.CENTER);
            setHorizontalTextPosition (SwingConstants.CENTER);
        }

        @Override  // ActionListener
        public abstract void actionPerformed (ActionEvent ae);

        @Override   // MouseListener
        public void mouseClicked(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseEntered(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseExited(MouseEvent e) { }

        @Override   // MouseListener
        public void mousePressed(MouseEvent e)
        {
            setIcon (buttonin);
        }

        @Override   // MouseListener
        public void mouseReleased(MouseEvent e)
        {
            setIcon (buttonout);
        }
    }
}
