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

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.Image;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import javax.imageio.ImageIO;
import javax.swing.Box;
import javax.swing.BoxLayout;
import javax.swing.ButtonGroup;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JScrollPane;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.border.BevelBorder;
import javax.swing.border.Border;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

public class GUI extends JPanel {

    public final static int UPDMS = 3;

    public final static Dimension buttondim = new Dimension (116, 116);
    public final static ImageIcon buttonin  = new ImageIcon (GUI.class.getClassLoader ().getResource ("violetcirc116.png"));
    public final static ImageIcon buttonout = new ImageIcon (GUI.class.getClassLoader ().getResource ("purplecirc116.png"));

    public final static Dimension leddim = new Dimension (58, 58);
    public final static ImageIcon ledon  = new ImageIcon (GUI.class.getClassLoader ().getResource ("violetcirc58.png"));
    public final static ImageIcon ledoff = new ImageIcon (GUI.class.getClassLoader ().getResource ("purplecirc58.png"));

    public final static Dimension procpandim = new Dimension (1044, 60);
    public final static Image     procpanimg = new ImageIcon (GUI.class.getClassLoader ().getResource ("procpan.png")).getImage ();

    public final static Dimension rl02pandim = new Dimension (1044, 50);
    public final static Image     rl02panimg = new ImageIcon (GUI.class.getClassLoader ().getResource ("rl02pan.png")).getImage ();

    public static BufferedImage redeyeim;
    public static JFrame mainframe;

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
        mainframe = new JFrame ("PDP-11/34A");
        mainframe.setDefaultCloseOperation (JFrame.EXIT_ON_CLOSE);
        mainframe.setContentPane (new GUI ());
        SwingUtilities.invokeLater (new Runnable () {
            @Override
            public void run ()
            {
                mainframe.pack ();
                mainframe.setLocationRelativeTo (null);
                mainframe.setVisible (true);

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

    public static MemCkBox bmckbox;
    public static DevCkBox dlckbox;
    public static DevCkBox kwckbox;
    public static DevCkBox kyckbox;
    public static DevCkBox rlckbox;

    public static RLDrive[] rldrives = new RLDrive[4];

    // update display with processor state - runs continuously
    // if processor is running, display current processor state
    // otherwise, leave display alone so ldad/exam/dep buttons will work
    public final static ActionListener updisplay =
        new ActionListener () {
            @Override
            public void actionPerformed (ActionEvent ae)
            {
                updatetimemillis = System.currentTimeMillis ();

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

                int fm = GUIZynqPage.pinget (fpgamodepin);
                if (fpgamode != fm) {
                    fpgamode = fm;
                    fpgamoderadiobuttons[0].update ();
                    fpgamoderadiobuttons[1].update ();
                    fpgamoderadiobuttons[2].update ();
                }
                bmckbox.update ();
                dlckbox.update ();
                kwckbox.update ();
                kyckbox.update ();
                rlckbox.update ();

                for (int i = 0; i < 4; i ++) rldrives[i].update ();
            }
        };

    public static int lastrunning = 12345;
    public static long updatetimemillis;

    public static int findpin (String name)
    {
        int p = GUIZynqPage.pinfind (name);
        if (p < 0) throw new RuntimeException ("pin " + name + " not found");
        return p;
    }

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

        ledbox.add (bits1715);
        ledbox.add (bits1412);
        ledbox.add (bits1109);
        ledbox.add (bits0806);
        ledbox.add (bits0503);
        ledbox.add (bits0200);

        bits1715.setLayout (new GridLayout (8, 3));
        bits1412.setLayout (new GridLayout (8, 3));
        bits1109.setLayout (new GridLayout (8, 3));
        bits0806.setLayout (new GridLayout (8, 3));
        bits0503.setLayout (new GridLayout (8, 3));
        bits0200.setLayout (new GridLayout (8, 3));

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
        bits0806.add (addrlbls[ 6] = centeredLabel (""));
        bits0503.add (addrlbls[ 5] = centeredLabel (""));
        bits0503.add (addrlbls[ 4] = centeredLabel ("A"));
        bits0503.add (addrlbls[ 3] = centeredLabel ("D"));
        bits0200.add (addrlbls[ 2] = centeredLabel ("D"));
        bits0200.add (addrlbls[ 1] = centeredLabel ("R"));
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
        bits1715.add (centeredLabel ("BERR"));
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
        buttonbox1.setBackground (Color.BLACK);
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

        ProcPanel procpanel = new ProcPanel ();
        add (procpanel);

        add (rldrives[0] = new RLDrive (0));
        add (rldrives[1] = new RLDrive (1));
        add (rldrives[2] = new RLDrive (2));
        add (rldrives[3] = new RLDrive (3));
    }

    public static JLabel centeredLabel (String label)
    {
        JLabel jl = new JLabel (label);
        jl.setHorizontalAlignment (JLabel.CENTER);
        return jl;
    }

    // panel what contains the messagelabel line and the fpgamode & device enable checkboxes
    public static class ProcPanel extends JPanel {
        public ProcPanel ()
        {
            setLayout (new BoxLayout (this, BoxLayout.Y_AXIS));
            setMaximumSize (procpandim);
            setMinimumSize (procpandim);
            setPreferredSize (procpandim);
            setSize (procpandim);

            Dimension fd = new Dimension (3, 3);
            add (new Box.Filler (fd, fd, fd));

            JPanel messagebox = new JPanel ();
            messagebox.setLayout (new BorderLayout ());
            Dimension mbd = new Dimension (1000, 22);
            messagebox.setMaximumSize (mbd);
            messagebox.setMinimumSize (mbd);
            messagebox.setPreferredSize (mbd);
            add (messagebox);

            messagelabel = centeredLabel (" ");
            messagebox.add (messagelabel, BorderLayout.CENTER);

            add (new Box.Filler (fd, fd, fd));

            ButtonGroup fmbg = new ButtonGroup ();
            fmbg.add (new FPGAModeRadioButton ("OFF",  0));
            fmbg.add (new FPGAModeRadioButton ("SIM",  1));
            fmbg.add (new FPGAModeRadioButton ("REAL    ", 2));

            JPanel ckboxrow = new JPanel ();
            ckboxrow.setLayout (new BoxLayout (ckboxrow, BoxLayout.X_AXIS));
            add (ckboxrow);
            ckboxrow.add (fpgamoderadiobuttons[0]);
            ckboxrow.add (fpgamoderadiobuttons[1]);
            ckboxrow.add (fpgamoderadiobuttons[2]);
            ckboxrow.add (bmckbox = new MemCkBox ("Mem/124KW    "));
            ckboxrow.add (dlckbox = new DevCkBox ("DL-11    ", "dl_enable"));
            ckboxrow.add (kwckbox = new DevCkBox ("KW-11    ", "kw_enable"));
            ckboxrow.add (kyckbox = new DevCkBox ("KY-11    ", "ky_enable"));
            ckboxrow.add (rlckbox = new RLDevCkBox ("RL-11"));
        }

        @Override
        protected void paintComponent (Graphics g)
        {
            super.paintComponent (g);
            g.drawImage (procpanimg, 0, 0, null);
        }
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
        public Timer blinker;

        @Override   // LED
        public void setOn (boolean on)
        {
            super.setOn (on);
            if ((blinker != null) && ! on) {
                blinker.stop ();
                blinker = null;
            }
            if ((blinker == null) && on) {
                blinker = new Timer (333, this);
                blinker.start ();
            }
        }

        @Override   // ActionListener
        public void actionPerformed (ActionEvent e)
        {
            super.setOn (! ison);
        }
    }

    public static class LED extends JButton {
        public boolean ison;

        public int dotxl, dotyt;

        public LED ()
        {
            setBorder (null);
            setContentAreaFilled (false);
            setIcon (ledoff);
            setMaximumSize (leddim);
            setMinimumSize (leddim);
            setPreferredSize (leddim);
            setRolloverEnabled (false);
            setSize (leddim);

            dotxl = (int) (leddim.getWidth  () - redeyeim.getWidth  ()) / 2 + 1;
            dotyt = (int) (leddim.getHeight () - redeyeim.getHeight ()) / 2 - 1;
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                setIcon (ison ? ledon : ledoff);
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
            setRolloverEnabled (true);
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

        @Override  // MemButton
        public void buttonClicked ()
        {
            berrled.setOn (false);
            messagelabel.setText ("stepping processor");
            GUIZynqPage.step ();                // tell processor to step single instruction
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
        public void buttonClicked ()
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
        public void buttonClicked ()
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
        public void buttonClicked ()
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
        public void buttonClicked ()
        {
            messagelabel.setText ("booting...");
            int switches = read18switches ();
            Thread t = new Thread () {
                @Override
                public void run ()
                {
                    // make sure no previous boot process is running
                    Process oldbp = bootprocess;
                    bootprocess = null;
                    if (oldbp != null) {
                        oldbp.destroyForcibly ();
                    }
                    try {

                        // start running guiboot.sh script
                        String ccode = System.getProperty ("ccode");
                        ProcessBuilder pb = new ProcessBuilder (ccode + "/guiboot.sh", Integer.toString (switches));
                        pb.redirectErrorStream (true);  // "2>&1"
                        bootprocess = pb.start ();

                        // forward its stdout & stderr to the message box
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
        public void buttonClicked ()
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
        public void buttonClicked ()
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
        if (loadedaddrvirt && (GUIZynqPage.running () <= 0)) {
            addrlbls[9].setText ("V");
            addrlbls[8].setText ("I");
            addrlbls[7].setText ("R");
            addrlbls[6].setText ("T");
        } else {
            addrlbls[9].setText ("P");
            addrlbls[8].setText ("H");
            addrlbls[7].setText ("Y");
            addrlbls[6].setText ("S");
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
        public void buttonClicked ()
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
        private boolean isenabled;

        public MemButton (String lbl)
        {
            super (lbl);

            addActionListener (this);
            addMouseListener (this);

            setBackground (Color.BLACK);
            setBorder (null);
            setContentAreaFilled (false);
            setForeground (Color.WHITE);
            setHorizontalTextPosition (SwingConstants.CENTER);
            setIcon (buttonout);
            setMaximumSize (buttondim);
            setMinimumSize (buttondim);
            setPreferredSize (buttondim);
            setSize (buttondim);
            setVerticalTextPosition (SwingConstants.CENTER);
        }

        public abstract void buttonClicked ();

        @Override
        public void setEnabled (boolean enable)
        {
            super.setEnabled (enable);
            isenabled = enable;
            if (enable) {
                setForeground (Color.WHITE);
                setIcon (buttonout);
            } else {
                setForeground (Color.DARK_GRAY);
                setIcon (null);
            }
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            if (isenabled) buttonClicked ();
        }

        @Override   // MouseListener
        public void mouseClicked(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseEntered(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseExited(MouseEvent e) { }

        @Override   // MouseListener
        public void mousePressed(MouseEvent e)
        {
            if (isenabled) setIcon (buttonin);
        }

        @Override   // MouseListener
        public void mouseReleased(MouseEvent e)
        {
            if (isenabled) setIcon (buttonout);
        }
    }

    ////////////////////////////////
    //  SIMULATED DEVICE CONTROL  //
    ////////////////////////////////

    // set fpga operating mode
    //  OFF  - everything disconnected from unibus, simulated devices held in reset
    //  SIM  - everything disconnected from unibus, simulated devices can be enabled, processor simulator enabled
    //  REAL - enabled devices connected to unibus, processor simulator held in reset
    public static FPGAModeRadioButton[] fpgamoderadiobuttons = new FPGAModeRadioButton[3];
    public static int fpgamode = -1;
    public static int fpgamodepin = findpin ("fpgamode");

    public static class FPGAModeRadioButton extends JRadioButton implements ChangeListener {
        public int value;

        public FPGAModeRadioButton (String label, int value)
        {
            super (label);
            this.value = value;
            fpgamoderadiobuttons[value] = this;
            addChangeListener (this);
        }

        public void update ()
        {
            setSelected (fpgamode == value);
        }

        @Override
        public void stateChanged (ChangeEvent e)
        {
            if (isSelected ()) {
                GUIZynqPage.pinset (fpgamodepin, value);
            }
        }
    }

    // memory enable checkbox
    // - manipulates bigmem.v enable bits
    public static class MemCkBox extends JCheckBox implements ChangeListener {
        public boolean isenab;
        public int enablopinindex, enabhipinindex;

        public MemCkBox (String label)
        {
            super (label);
            enablopinindex = findpin ("bm_enablo");
            enabhipinindex = findpin ("bm_enabhi");
            addChangeListener (this);
        }

        // called repeatedly by updisplay() to make sure checkbox matches fpga enables
        // it should not modify the enable bits
        public void update ()
        {
            // see what fpga is showing for enables
            boolean sbenab = (GUIZynqPage.pinget (enabhipinindex) != 0) || (GUIZynqPage.pinget (enablopinindex) != 0);

            // if different that what checkbox shows, update checkbox
            // triggers a call to stateChanged(), so update isenab first
            if (isenab != sbenab) {
                isenab = sbenab;
                setSelected (isenab);
            }
        }

        // called when the user clicks the checkbox to enable/disable fpga memory
        // also called when different checkbox detected from fpga memory (because setSelected() was called)
        // we want to change the enable bits only when user changed checkbox, not when we get incoming change
        // if disabling, all enable bits are cleared
        // if enabling,
        //   if real mode, fills in for missing memory
        //           else, all enables are turned on
        @Override   // ChangeListener
        public void stateChanged (ChangeEvent e)
        {
            boolean en = isSelected ();
            if (isenab != en) {
                isenab = en;
                long mask = 0;                                          // assume disabling everything
                if (isenab) {
                    mask = (1L << 62) - 1;                              // assume enabling everything
                    if (fpgamode == 2) {                                // sim or off, enable everyting
                        GUIZynqPage.pinset (enablopinindex, 0);         // real, turn off all fpga memory
                        GUIZynqPage.pinset (enabhipinindex, 0);
                        for (int page = 0; page < 62; page ++) {        // loop through all possible 4KB pages
                            int rc = GUIZynqPage.rdmem (page << 12);    // see if first word of page readable
                            if (rc != -1) mask &= ~ (1L << page);       // if timed out, we need that page
                        }
                        if (mask == 0) {
                            messagelabel.setText ("processor has 124KW memory, no fpga memory enabled");
                            isenab = false;
                            setSelected (false);
                            return;
                        }
                    }
                }
                messagelabel.setText (String.format ("fpga memory enable mask %016X", mask));
                GUIZynqPage.pinset (enablopinindex, (int) mask);
                GUIZynqPage.pinset (enabhipinindex, (int) (mask >> 32));
            }
        }
    }

    public static class RLDevCkBox extends DevCkBox {

        public RLDevCkBox (String label)
        {
            super (label, "rl_enable");
        }

        @Override   // DevCkBox
        public void stateChanged (ChangeEvent e)
        {
            super.stateChanged (e);

            for (RLDrive rldrive : rldrives) {
                rldrive.setVisible (isenab);
            }
            mainframe.pack ();
        }
    }

    // device enable checkbox
    // - plugs/unplugs simulated circuit board from unibus
    public static class DevCkBox extends JCheckBox implements ChangeListener {
        public boolean isenab;
        public int enabpinindex;

        public DevCkBox (String label, String enabpin)
        {
            super (label);
            enabpinindex = findpin (enabpin);
            addChangeListener (this);
        }

        public void update ()
        {
            boolean sbenab = GUIZynqPage.pinget (enabpinindex) != 0;
            if (isenab != sbenab) {
                isenab = sbenab;
                setSelected (isenab);
            }
        }

        @Override
        public void stateChanged (ChangeEvent e)
        {
            boolean en = isSelected ();
            if (isenab != en) {
                isenab = en;
                GUIZynqPage.pinset (enabpinindex, isenab ? 1 : 0);
            }
        }
    }

    /////////////////
    //  RL DRIVES  //
    /////////////////

    public static File rlchooserdirectory;

    public static class RLButton extends JButton {
        public boolean ison;
        public Color offcolor;
        public Color oncolor;

        public RLButton (String label, Color oncolor, Color offcolor)
        {
            super (label);
            this.offcolor = offcolor;
            this.oncolor  = oncolor;
            //Dimension d = new Dimension (80, 80);
            //setPreferredSize (d);
            setBackground (offcolor);
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                setBackground (ison ? oncolor : offcolor);
            }
        }
    }

    public static class RLDrive extends JPanel {
        public int drive;
        public int lastcylno;
        public int lastfnseq;
        public JLabel cylnolbl;
        public JLabel rlmessage;
        public long blockmsgupdates;
        public long winkoutready;
        public RLButton loadbutton;
        public RLButton readylight;
        public RLButton faultlight;
        public RLButton wprtswitch;

        public final static int RLDISKSIZE = 512*2*40*256;  // bytes in disk file

        public RLDrive (int d)
        {
            drive = d;
            lastcylno = -1;
            lastfnseq = -1;

            setLayout (new BoxLayout (this, BoxLayout.X_AXIS));
            setMaximumSize (rl02pandim);
            setMinimumSize (rl02pandim);
            setPreferredSize (rl02pandim);
            setSize (rl02pandim);

            add (cylnolbl   = new JLabel ("       "));
            add (loadbutton = new RLButton ("LOAD",  Color.YELLOW, Color.GRAY));
            add (readylight = new RLButton (drive + " RDY", Color.WHITE, Color.GRAY));
            add (faultlight = new RLButton ("FAULT", Color.RED,    Color.GRAY));
            add (wprtswitch = new RLButton ("WRPRT", Color.YELLOW, Color.GRAY));
            add (new JLabel ("   "));
            add (rlmessage  = new JLabel (""));

            Font lf = new Font ("Monospaced", Font.PLAIN, cylnolbl.getFont ().getSize ());
            cylnolbl.setFont (lf);

            Dimension md = new Dimension (600, 50);
            rlmessage.setMaximumSize (md);
            rlmessage.setMinimumSize (md);
            rlmessage.setPreferredSize (md);
            rlmessage.setSize (md);

            // do something when LOAD button is clicked
            loadbutton.addActionListener (new ActionListener () {
                public void actionPerformed (ActionEvent ae)
                {
                    if ((GUIZynqPage.rlstat (drive) & GUIZynqPage.RLSTAT_LOAD) == 0) {

                        // display chooser box to select file to load
                        JFileChooser chooser = new JFileChooser ();
                        chooser.setDialogTitle ("Loading RL drive " + drive);
                        if (rlchooserdirectory != null) chooser.setCurrentDirectory (rlchooserdirectory);
                        int rc = chooser.showOpenDialog (RLDrive.this);
                        if (rc == JFileChooser.APPROVE_OPTION) {

                            // file selected, pass to z11rl via shared memory
                            File   ff = chooser.getSelectedFile ();
                            rlchooserdirectory = ff.getParentFile ();
                            long size = ff.length ();
                            if ((size == RLDISKSIZE) || (JOptionPane.showConfirmDialog (RLDrive.this,
                                    ff.getName () + " size " + size + " is not " + RLDISKSIZE + "\nAre you sure you want to load it?",
                                    "Loading Drive", JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                                String fn = ff.getAbsolutePath ();
                                String er = GUIZynqPage.rlload (drive, wprtswitch.ison, fn);
                                if (er != null) {
                                    rlmessage.setText ("error loading: " + er);
                                    blockmsgupdates = System.currentTimeMillis () + 5000;
                                    loadbutton.setOn (false);
                                } else {
                                    rlmessage.setText (fn);
                                    loadbutton.setOn (true);
                                }
                            }
                        }
                    } else {

                        // unload whatever is in there
                        if (JOptionPane.showConfirmDialog (RLDrive.this,
                                "Are you sure you want to unload drive " + drive + "?",
                                "Unloading Drive",
                                JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION) {
                            GUIZynqPage.rlload (drive, wprtswitch.ison, null);
                            rlmessage.setText ("");
                            loadbutton.setOn (false);
                        }
                    }
                }
            });

            // do something when WRPRT button is clicked
            // allow changes only when unloaded
            wprtswitch.addActionListener (new ActionListener () {
                public void actionPerformed (ActionEvent ae)
                {
                    if ((GUIZynqPage.rlstat (drive) & GUIZynqPage.RLSTAT_LOAD) == 0) {
                        GUIZynqPage.rlload (drive, ! wprtswitch.ison, null);
                    } else {
                        rlmessage.setText ("write protect change allowed only when unloaded");
                        blockmsgupdates = System.currentTimeMillis () + 5000;
                    }
                }
            });
        }

        // update buttons and text to match fpga & shared memory
        public void update ()
        {
            int stat = GUIZynqPage.rlstat (drive);

            // update cylinder number if it changed, blanks if drive not loaded
            // also wink out the drive ready light
            int thiscylno = ((stat & GUIZynqPage.RLSTAT_LOAD) == 0) ? -1 : (stat & GUIZynqPage.RLSTAT_CYLNO) / (GUIZynqPage.RLSTAT_CYLNO & - GUIZynqPage.RLSTAT_CYLNO);
            if (lastcylno != thiscylno) {
                lastcylno = thiscylno;
                cylnolbl.setText ((lastcylno < 0) ? "       " : String.format ("  %03d  ", lastcylno));
                stat &= ~ GUIZynqPage.RLSTAT_READY;
            }

            // stretch the not-ready status out so it blinks when there is a seek
            if ((stat & GUIZynqPage.RLSTAT_READY) == 0) winkoutready = updatetimemillis + 50;

            // update load, ready, fault, write protect lights
            loadbutton.setOn ((stat & GUIZynqPage.RLSTAT_LOAD)  != 0);
            readylight.setOn (updatetimemillis >= winkoutready);
            faultlight.setOn ((stat & GUIZynqPage.RLSTAT_FAULT) != 0);
            wprtswitch.setOn ((stat & GUIZynqPage.RLSTAT_WRPRT) != 0);

            // update loaded filename if there was a change and if no error message is being displayed
            int thisfnseq = stat & GUIZynqPage.RLSTAT_FNSEQ;
            if ((lastfnseq != thisfnseq) && ((blockmsgupdates == 0) || (updatetimemillis > blockmsgupdates))) {
                rlmessage.setText (GUIZynqPage.rlfile (drive));
                blockmsgupdates = 0;
                lastfnseq = thisfnseq;
            }
        }

        @Override
        protected void paintComponent (Graphics g)
        {
            super.paintComponent (g);
            g.drawImage (rl02panimg, 0, 0, null);
        }
    }
}
