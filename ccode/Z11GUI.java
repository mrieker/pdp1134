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
//  ./z11gui

// apt install default-jdk
// ln -s /usr/lib/jvm/java-11-openjdk-armhf /opt/jdk

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.Image;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
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
import javax.swing.filechooser.FileNameExtensionFilter;

public class Z11GUI extends JPanel {

    public final static int UPDMS = 23;     // updisplay() typically takes 1mS

    public final static Dimension buttondim = new Dimension (116, 116);
    public final static ImageIcon buttonin  = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("violetcirc116.png"));
    public final static ImageIcon buttonout = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("purpleclear116.png"));

    public final static Dimension leddim = new Dimension (58, 58);
    public final static ImageIcon ledon  = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("purpleflat58.png"));
    public final static ImageIcon ledoff = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("purpleflat58.png"));
    public final static ImageIcon swton  = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("violetcirc58.png"));
    public final static ImageIcon swtoff = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("purpleclear58.png"));

    public final static Dimension procpandim = new Dimension (1044, 60);
    public final static Image     procpanimg = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("procpan.png")).getImage ();

    public final static Dimension rl02pandim = new Dimension (1044, 50);
    public final static Image     rl02panimg = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("rl02pan.png")).getImage ();

    public final static Image     pdplogoimg = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("pdplogo.png")).getImage ();

    public final static Dimension redeyedim  = new Dimension (36, 36);
    public final static Image     redeyeimg  = new ImageIcon (Z11GUI.class.getClassLoader ().getResource ("redleda36.png")).getImage ();

    public static JFrame mainframe;
    public static Toolkit toolkit;

    public static void main (String[] args)
            throws Exception
    {
        if (args.length > 0) {
            System.err.println ("unknown argument/option " + args[0]);
            System.exit (1);
        }

        // open access to Zynq board
        GUIZynqPage.open ();

        // create window and show it
        mainframe = new JFrame ("PDP-11/34A");
        mainframe.setDefaultCloseOperation (JFrame.EXIT_ON_CLOSE);
        mainframe.setContentPane (new Z11GUI ());
        SwingUtilities.invokeLater (new Runnable () {
            @Override
            public void run ()
            {
                mainframe.pack ();
                mainframe.setLocationRelativeTo (null);
                mainframe.setVisible (true);
                toolkit = mainframe.getToolkit ();

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

    public static FModeCkBox fmckbox;
    public static MemCkBox   bmckbox;
    public static DevCkBox   dlckbox;
    public static DZCkBox    dzckbox;
    public static KWCkBox    kwckbox;
    public static DevCkBox   kyckbox;
    public static DevCkBox   pcckbox;
    public static DevCkBox   rlckbox;
    public static DevCkBox   tmckbox;

    public static RLDrive[] rldrives = new RLDrive[4];
    public static TMDrive[] tmdrives = new TMDrive[2];

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

                // light register lights also always reflect the fpga 777570 register
                writelregleds (sample_lreg);

                // same with switch register - in case z11ctrl or similar flipped them
                write18switches (sample_sreg);

                // update grayed buttons based on running state and console enabled
                //  running = +1 : processor is running
                //             0 : processor halted but is resumable
                //            -1 : processor halted, reset required
                //  ky11enab = true : using fpga console
                //            false : using real PDP console

                // the memory access buttons require processor to be halted
                // they do not work with real KY-11 present cuz it blocks dma
                boolean memenabs = (sample_running <= 0) && lastky11enab;
                ldadbutton.setEnabled  (memenabs);
                exambutton.setEnabled  (memenabs);
                depbutton.setEnabled   (memenabs);

                // the cont & step buttons require process be halted and resumable
                // they do not seem to work with real KY-11 probably cuz of halt line
                // start button does its own reset so halt does not need to be resumable
                boolean ctlenabs = (sample_running == 0) && lastky11enab;
                stepbutton.setEnabled  (ctlenabs);
                contbutton.setEnabled  (ctlenabs);
                startbutton.setEnabled (memenabs);

                // these buttons function regardless of console status
                // - reset should always work so just leave it defaulted to enabled
                // - halt only makes sense if processor is running
                //   it does not work with real KY-11 in place cuz of halt line
                // - boot only works if processor is halted
                haltbutton.setEnabled  ((sample_running  > 0) && lastky11enab);
                bootbutton.setEnabled   (sample_running <= 0);

                // if processor currently running, update lights from what fpga last captured from unibus
                if (sample_running > 0) {
                    writeaddrleds (sample_addr);
                    writedataleds (sample_data);
                    berrled.setOn (false);
                }

                // if processor just halted, display PC,PS
                else if ((lastrunning > 0) && lastky11enab) {
                    int r0 = GUIZynqPage.rdmem (0777700);
                    int pc = GUIZynqPage.rdmem (0777707);
                    int ps = GUIZynqPage.rdmem (0777776);
                    StringBuilder sb = new StringBuilder ();
                    if (pc >= 0) {
                        sb.append (String.format ("stopped at PC %06o", pc));
                        int pcpa = vatopa (pc, ps >> 14);
                        berrled.setOn (pcpa < 0);
                        if (pcpa < 0) {
                            sb.append (", ");
                            sb.append (vatopaerr[~pcpa]);
                        } else if (pcpa != pc) {
                            sb.append (String.format (" (pa %06o)", pcpa));
                        }
                        loadedaddress = pc;
                        if (ps >= 0) loadedaddress |= (ps & 0140000) << 2;
                        loadedaddrvirt = true;
                        depbutton.autoincrement = false;
                        exambutton.autoincrement = false;
                        writeaddrleds (loadedaddress);
                    } else {
                        writeaddrleds (-1);
                        sb.append ("stopped at PC unknown");
                    }
                    if (ps >= 0) {
                        sb.append (String.format (", PS %06o", ps));
                    } else {
                        sb.append (", PS unknown");
                    }
                    if (r0 >= 0) {
                        writedataleds (r0);
                        sb.append (String.format (", R0 %06o", r0));
                    } else {
                        writedataleds (-1);
                        sb.append (", RO unknown");
                    }
                    messagelabel.setText (sb.toString ());
                }

                // update ADDRS label with PHYS or VIRT or nothing
                if (lastrunning != sample_running) {
                    lastrunning = sample_running;
                    updateaddrlabel ();
                }

                // update option selection checkboxes
                fmckbox.update ();
                bmckbox.update ();
                dlckbox.update ();
                dzckbox.update ();
                kwckbox.update ();
                kyckbox.update ();
                pcckbox.update ();
                rlckbox.update ();
                tmckbox.update ();

                // remove lights & switches if KY-11 is disabled
                if (lastky11enab != kyckbox.isenab) {
                    lastky11enab = kyckbox.isenab;
                    updateaddrlabel ();
                    setConsoleLights (lastky11enab);
                }

                // flush updates to screen
                toolkit.sync ();
            }
        };

    public static boolean lastky11enab = true;
    public static int lastrunning = 12345;
    public static long updatetimemillis;

    public static int findpin (String name)
    {
        int p = GUIZynqPage.pinfind (name);
        if (p < 0) throw new RuntimeException ("pin " + name + " not found");
        return p;
    }

    public static JPanel bits1715;
    public static JPanel bits1412;
    public static JPanel bits1109;
    public static JPanel bits0806;
    public static JPanel bits0503;
    public static JPanel bits0200;

    // build the display
    public Z11GUI ()
    {
        setLayout (new BoxLayout (this, BoxLayout.Y_AXIS));

        JPanel ledbox = new JPanel ();
        ledbox.setLayout (new BoxLayout (ledbox, BoxLayout.X_AXIS));
        add (ledbox);

        bits1715 = new JPanel () {
            @Override
            protected void paintComponent (Graphics g)
            {
                super.paintComponent (g);
                g.drawImage (pdplogoimg, 5, 8, null);
            }
        };

        bits1412 = new JPanel ();
        bits1109 = new JPanel ();
        bits0806 = new JPanel ();
        bits0503 = new JPanel ();
        bits0200 = new JPanel ();

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

        bits1715.setLayout (new GridLayout (0, 3));
        bits1412.setLayout (new GridLayout (0, 3));
        bits1109.setLayout (new GridLayout (0, 3));
        bits0806.setLayout (new GridLayout (0, 3));
        bits0503.setLayout (new GridLayout (0, 3));
        bits0200.setLayout (new GridLayout (0, 3));

        // row 0 - address label
        bits1715.add (addrlbls[17] = lightsLabel (""));
        bits1715.add (addrlbls[16] = lightsLabel (""));
        bits1715.add (addrlbls[15] = lightsLabel (""));
        bits1412.add (addrlbls[14] = lightsLabel (""));
        bits1412.add (addrlbls[13] = lightsLabel (""));
        bits1412.add (addrlbls[12] = lightsLabel (""));
        bits1109.add (addrlbls[11] = lightsLabel (""));
        bits1109.add (addrlbls[10] = lightsLabel (""));
        bits1109.add (addrlbls[ 9] = lightsLabel (""));
        bits0806.add (addrlbls[ 8] = lightsLabel (""));
        bits0806.add (addrlbls[ 7] = lightsLabel (""));
        bits0806.add (addrlbls[ 6] = lightsLabel (""));
        bits0503.add (addrlbls[ 5] = lightsLabel (""));
        bits0503.add (addrlbls[ 4] = lightsLabel ("A"));
        bits0503.add (addrlbls[ 3] = lightsLabel ("D"));
        bits0200.add (addrlbls[ 2] = lightsLabel ("D"));
        bits0200.add (addrlbls[ 1] = lightsLabel ("R"));
        bits0200.add (addrlbls[ 0] = lightsLabel ("S"));

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
        bits1715.add (lightsLabel ("RUN"));
        bits1715.add (lightsLabel (""));
        bits1715.add (lightsLabel (""));
        bits1412.add (lightsLabel (""));
        bits1412.add (lightsLabel (""));
        bits1412.add (lightsLabel (""));
        bits1109.add (lightsLabel (""));
        bits1109.add (lightsLabel (""));
        bits1109.add (lightsLabel (""));
        bits0806.add (lightsLabel (""));
        bits0806.add (lightsLabel (""));
        bits0806.add (lightsLabel (""));
        bits0503.add (lightsLabel (""));
        bits0503.add (lightsLabel (""));
        bits0503.add (lightsLabel ("D"));
        bits0200.add (lightsLabel ("A"));
        bits0200.add (lightsLabel ("T"));
        bits0200.add (lightsLabel ("A"));

        // row 3 - data bits
        bits1715.add (runled = new LED ());
        bits1715.add (lightsLabel (""));
        bits1715.add (dataleds[15] = new LED ());
        for (int i = 3; -- i >= 0;) {
            bits1412.add (dataleds[12+i] = new LED ());
            bits1109.add (dataleds[ 9+i] = new LED ());
            bits0806.add (dataleds[ 6+i] = new LED ());
            bits0503.add (dataleds[ 3+i] = new LED ());
            bits0200.add (dataleds[ 0+i] = new LED ());
        }

        // lights and switches rows
        initConsoleLights ();

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
        add (tmdrives[0] = new TMDrive (0));
        add (tmdrives[1] = new TMDrive (1));
    }

    public static JLabel lightsLabel (String label)
    {
        JLabel jl = new JLabel (label);
        jl.setHorizontalAlignment (JLabel.CENTER);
        jl.setForeground (Color.WHITE);
        return jl;
    }

    // set up console 777570 lights & switch rows
    public static void initConsoleLights ()
    {
        consolelights = new JComponent[72];

        int j = 0;
        consolelights[j++] = lightsLabel ("BERR");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("L");
        consolelights[j++] = lightsLabel ("I");
        consolelights[j++] = lightsLabel ("G");
        consolelights[j++] = lightsLabel ("H");
        consolelights[j++] = lightsLabel ("T");
        consolelights[j++] = lightsLabel ("S");

        // row 5 - 777570 lights
        consolelights[j++] = berrled = new FlashingLED ();
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lregleds[15] = new LED ();
        for (int i = 3; -- i >= 0;) {
            consolelights[j++] = lregleds[12+i] = new LED ();
            consolelights[j++] = lregleds[ 9+i] = new LED ();
            consolelights[j++] = lregleds[ 6+i] = new LED ();
            consolelights[j++] = lregleds[ 3+i] = new LED ();
            consolelights[j++] = lregleds[ 0+i] = new LED ();
        }

        // row 6 - switches label
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("");
        consolelights[j++] = lightsLabel ("S");
        consolelights[j++] = lightsLabel ("W");
        consolelights[j++] = lightsLabel ("I");
        consolelights[j++] = lightsLabel ("T");
        consolelights[j++] = lightsLabel ("C");
        consolelights[j++] = lightsLabel ("H");
        consolelights[j++] = lightsLabel ("E");
        consolelights[j++] = lightsLabel ("S");

        // row 7 - switch register
        for (int i = 3; -- i >= 0;) {
            consolelights[j++] = switches[15+i] = new Switch (15+i);
            consolelights[j++] = switches[12+i] = new Switch (12+i);
            consolelights[j++] = switches[ 9+i] = new Switch ( 9+i);
            consolelights[j++] = switches[ 6+i] = new Switch ( 6+i);
            consolelights[j++] = switches[ 3+i] = new Switch ( 3+i);
            consolelights[j++] = switches[ 0+i] = new Switch ( 0+i);
        }

        // row 4 - 777570 lights label
        j = 0;
        bits1715.add (consolelights[j++]);      // lightsLabel ("BERR");
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits0806.add (consolelights[j++]);      // lightsLabel ("");
        bits0806.add (consolelights[j++]);      // lightsLabel ("");
        bits0806.add (consolelights[j++]);      // lightsLabel ("");
        bits0503.add (consolelights[j++]);      // lightsLabel ("L");
        bits0503.add (consolelights[j++]);      // lightsLabel ("I");
        bits0503.add (consolelights[j++]);      // lightsLabel ("G");
        bits0200.add (consolelights[j++]);      // lightsLabel ("H");
        bits0200.add (consolelights[j++]);      // lightsLabel ("T");
        bits0200.add (consolelights[j++]);      // lightsLabel ("S");

        // row 5 - 777570 lights
        bits1715.add (consolelights[j++]);      // berrled = new FlashingLED ();
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1715.add (consolelights[j++]);      // lregleds[15] = new LED ();
        for (int i = 3; -- i >= 0;) {
            bits1412.add (consolelights[j++]);  // lregleds[12+i] = new LED ();
            bits1109.add (consolelights[j++]);  // lregleds[ 9+i] = new LED ();
            bits0806.add (consolelights[j++]);  // lregleds[ 6+i] = new LED ();
            bits0503.add (consolelights[j++]);  // lregleds[ 3+i] = new LED ();
            bits0200.add (consolelights[j++]);  // lregleds[ 0+i] = new LED ();
        }

        // row 6 - switches label
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1715.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1412.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits1109.add (consolelights[j++]);      // lightsLabel ("");
        bits0806.add (consolelights[j++]);      // lightsLabel ("");
        bits0806.add (consolelights[j++]);      // lightsLabel ("S");
        bits0806.add (consolelights[j++]);      // lightsLabel ("W");
        bits0503.add (consolelights[j++]);      // lightsLabel ("I");
        bits0503.add (consolelights[j++]);      // lightsLabel ("T");
        bits0503.add (consolelights[j++]);      // lightsLabel ("C");
        bits0200.add (consolelights[j++]);      // lightsLabel ("H");
        bits0200.add (consolelights[j++]);      // lightsLabel ("E");
        bits0200.add (consolelights[j++]);      // lightsLabel ("S");

        // row 7 - switch register
        for (int i = 3; -- i >= 0;) {
            bits1715.add (consolelights[j++]);  // switches[15+i] = new Switch (15+i);
            bits1412.add (consolelights[j++]);  // switches[12+i] = new Switch (12+i);
            bits1109.add (consolelights[j++]);  // switches[ 9+i] = new Switch ( 9+i);
            bits0806.add (consolelights[j++]);  // switches[ 6+i] = new Switch ( 6+i);
            bits0503.add (consolelights[j++]);  // switches[ 3+i] = new Switch ( 3+i);
            bits0200.add (consolelights[j++]);  // switches[ 0+i] = new Switch ( 0+i);
        }
    }

    public static Container[] consolecontainers;
    public static JComponent[] consolelights;

    // turn 777570 lights & switch rows on/off if KY-11 is enabled/disabled
    public static void setConsoleLights (boolean kyenabled)
    {
        if (consolecontainers == null) {
            consolecontainers = new Container[72];
            for (int i = 0; i < 72; i ++) {
                consolecontainers[i] = consolelights[i].getParent ();
            }
        }

        if (kyenabled) {
            for (int i = 0; i < 72; i ++) {
                consolecontainers[i].add (consolelights[i]);
            }
        } else {
            for (int i = 0; i < 72; i ++) {
                consolecontainers[i].remove (consolelights[i]);
            }
        }

        mainframe.pack ();
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

            messagelabel = new JLabel (" ");
            messagelabel.setHorizontalAlignment (JLabel.CENTER);
            messagebox.add (messagelabel, BorderLayout.CENTER);

            add (new Box.Filler (fd, fd, fd));

            JPanel ckboxrow = new JPanel ();
            ckboxrow.setLayout (new BoxLayout (ckboxrow, BoxLayout.X_AXIS));
            add (ckboxrow);
            ckboxrow.add (fmckbox = new FModeCkBox ("OFF   "));
            ckboxrow.add (bmckbox = new MemCkBox   ("Mem   "));
            ckboxrow.add (dlckbox = new DevCkBox   ("DL-11   ", "dl_enable"));
            ckboxrow.add (dzckbox = new DZCkBox    ("DZ-11   "));
            ckboxrow.add (kwckbox = new KWCkBox    ("KW-11   "));
            ckboxrow.add (kyckbox = new DevCkBox   ("KY-11   ", "ky_enable"));
            ckboxrow.add (pcckbox = new DevCkBox   ("PC-11   ", "pc_enable"));
            ckboxrow.add (rlckbox = new RLDevCkBox ("RL-11   "));
            ckboxrow.add (tmckbox = new TMDevCkBox ("TM-11"));
        }

        @Override
        protected void paintComponent (Graphics g)
        {
            super.paintComponent (g);
            g.drawImage (procpanimg, 0, 0, null);
        }
    }

    // write 18-bit value to SR (switch register) leds
    public static void write18switches (int sr)
    {
        for (int i = 0; i < 18; i ++) {
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

        public ImageIcon iconon  = ledon;
        public ImageIcon iconoff = ledoff;

        public LED ()
        {
            setBorder (null);
            setContentAreaFilled (false);
            setIcon (iconoff);
            setMaximumSize (leddim);
            setMinimumSize (leddim);
            setPreferredSize (leddim);
            setRolloverEnabled (false);
            setSize (leddim);

            dotxl = (int) (leddim.getWidth  () - redeyedim.getWidth  ()) / 2;
            dotyt = (int) (leddim.getHeight () - redeyedim.getHeight ()) / 2;
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                setIcon (ison ? iconon : iconoff);
            }
        }

        @Override
        public void paint (Graphics g)
        {
            super.paint (g);
            if (ison) {
                g.drawImage (redeyeimg, dotxl, dotyt, null);
            }
        }
    }

    public static class Switch extends LED implements ActionListener {
        public int mask;

        public Switch (int n)
        {
            mask = 1 << n;
            setRolloverEnabled (true);
            addActionListener (this);
            iconon  = swton;
            iconoff = swtoff;
            setIcon (iconoff);
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            setOn (! ison);

            // switch flipped, update FPGA 777570 switch register
            // FPGA also has storage for bits 16 & 17
            int sr = GUIZynqPage.getsr ();
            sr = ison ? (sr | mask) : (sr & ~ mask);
            GUIZynqPage.setsr (sr);
        }
    }

    ///////////////////////////////
    // PROCESSOR CONTROL BUTTONS //
    ///////////////////////////////

    public static class StepButton extends MemButton {
        public StepButton ()
        {
            super ("STEP");
            setToolTipText ("process single instruction");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            berrled.setOn (false);
            messagelabel.setText ("stepping processor");
            GUIZynqPage.step ();                // tell processor to step single instruction
            checkforhalt ();                    // make sure it halted
            lastrunning = 12345;                // force updisplay to refresh everything
        }
    }

    // - stop processing instructions
    public static class HaltButton extends MemButton {
        public HaltButton ()
        {
            super ("HALT");
            setToolTipText ("stop processing instructions");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            berrled.setOn (false);
            messagelabel.setText ("halting processor");
            GUIZynqPage.halt ();                // tell processor to stop executing instructions
            checkforhalt ();                    // make sure it halted
        }
    }

    // - continue processing instructions
    public static class ContButton extends MemButton {
        public ContButton ()
        {
            super ("CONT");
            setToolTipText ("continue processing instructions");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            berrled.setOn (false);
            messagelabel.setText ("resuming processor");
            GUIZynqPage.cont ();
            messagelabel.setText ("processor resumed");
        }
    }

    // - reset I/O devices and processor
    public static class ResetButton extends MemButton {
        public ResetButton ()
        {
            super ("RESET");
            setEnabled (true);
            setToolTipText ("reset processor and halt");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            if ((GUIZynqPage.running () <= 0) ||
                longclick ||
                (JOptionPane.showConfirmDialog (this,
                        "Are you sure you want to reset the system?",
                        "RESET Button",
                        JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                berrled.setOn (false);
                messagelabel.setText ("resetting processor");
                GUIZynqPage.reset ();
                checkforhalt ();
                messagelabel.setText ("processor reset complete");
            }
        }
    }

    // - boot processor
    public static class BootButton extends MemButton {
        public Process bootprocess;

        public BootButton ()
        {
            super ("BOOT");
            setToolTipText ("boot processor via guiboot.sh");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            messagelabel.setText ("booting...");
            int switches = GUIZynqPage.getsr ();
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
                        ProcessBuilder pb = new ProcessBuilder (ccode + "/guiboot.sh",
                                Integer.toString (switches), Integer.toString (loadedaddress));
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
            setToolTipText ("reset then start processing instructions");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
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
        public LdAdButton ()
        {
            super ("LD.AD");
            setToolTipText ("load address for exam/dep/start; long click for virt addr with mode in <17:16>");
        }

        @Override  // MemButton
        public void buttonClicked (boolean longclick)
        {
            messagelabel.setText ("");
            depbutton.autoincrement = false;
            exambutton.autoincrement = false;
            loadedaddress = GUIZynqPage.getsr () & 0777777;
            if ((loadedaddress < 0777700) || (loadedaddress > 0777717)) {
                loadedaddress &= 0777776;
            }
            writeaddrleds (loadedaddress);
            writedataleds (0);
            loadedaddrvirt = longclick;
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
    }

    // update VIRT/PHYS tag on address lights label
    public static void updateaddrlabel ()
    {
        if (! lastky11enab) {
            addrlbls[9].setText ("");
            addrlbls[8].setText ("");
            addrlbls[7].setText ("");
            addrlbls[6].setText ("");
        } else if (loadedaddrvirt && (GUIZynqPage.running () <= 0)) {
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
            setToolTipText ("examine memory location");
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
            setToolTipText ("write memory location from switches");
        }

        @Override  // ExDepButton
        public String memop ()
        {
            return "deposited to";
        }

        @Override  // ExDepButton
        public int rwmem (int pa)
        {
            int data = GUIZynqPage.getsr () & 0177777;
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
        public void buttonClicked (boolean longclick)
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
                    switch (rc) {
                        case GUIZynqPage.DMAERR_TIMO: st += ", dma timed out"; break;
                        case GUIZynqPage.DMAERR_PARE: st += ", parity error"; break;
                        case GUIZynqPage.DMAERR_STUK: st += ", dma blocked"; break;
                        default: st += ", error " + rc; break;
                    }
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

    public static abstract class MemButton extends JButton implements MouseListener {
        private boolean isenabled;
        private boolean pressedinarea;
        private long pressedat;

        public MemButton (String lbl)
        {
            super (lbl);

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

        public abstract void buttonClicked (boolean longclick);

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

        @Override   // MouseListener
        public void mouseClicked(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseEntered(MouseEvent e) { }

        @Override   // MouseListener
        public void mouseExited(MouseEvent e) { }

        @Override   // MouseListener
        public void mousePressed(MouseEvent e)
        {
            if (isenabled) {
                pressedinarea = eventInArea (e);
                pressedat = System.currentTimeMillis ();
                if (pressedinarea) setIcon (buttonin);
            }
        }

        @Override   // MouseListener
        public void mouseReleased(MouseEvent e)
        {
            if (isenabled && pressedinarea && eventInArea (e)) {
                long releasedat = System.currentTimeMillis ();
                buttonClicked ((releasedat - pressedat) >= 800);
            }
            setIcon (buttonout);
            pressedinarea = false;
        }

        // see if mouse is within the circle itself
        // otherwise it gets clicks on the black border area
        private boolean eventInArea (MouseEvent e)
        {
            int w = (int) buttondim.getWidth ();
            int h = (int) buttondim.getHeight ();
            return sq (e.getX () - w / 2) + sq (e.getY () - h / 2) < sq ((w + h) / 6);
        }

        private static int sq (int x) { return x * x; }
    }

    ////////////////////////////////
    //  SIMULATED DEVICE CONTROL  //
    ////////////////////////////////

    // set fpga operating mode
    //  OFF  - everything disconnected from unibus, simulated devices held in reset
    //  SIM  - everything disconnected from unibus, simulated devices can be enabled, processor simulator enabled
    //  REAL - enabled devices connected to unibus, processor simulator held in reset
    public final static int FM_OFF  = 0;
    public final static int FM_SIM  = 1;
    public final static int FM_REAL = 2;
    public static int fpgamode = -1;

    public static class FModeCkBox extends JCheckBox implements ItemListener {
        private boolean suppress;
        private boolean waschecked;
        private boolean wasenabled;
        private boolean wasfiftyhz;
        private int fpgamodepinindex;
        private String offlabel;
        private String realabel;
        private String simlabel;

        public FModeCkBox (String label)
        {
            super (label);
            fpgamodepinindex = findpin ("fpgamode");
            offlabel = label;
            realabel = label.replace ("OFF", "REAL");
            simlabel = label.replace ("OFF", "SIM");
            addItemListener (this);
        }

        // called many times a second to make sure checkbox matches fpga
        public void update ()
        {
            int ismode = GUIZynqPage.pinget (fpgamodepinindex);
            if (fpgamode != ismode) {
                fpgamode = ismode;
                switch (fpgamode) {
                    case FM_OFF:  setText (offlabel); break;
                    case FM_SIM:  setText (simlabel); break;
                    case FM_REAL: setText (realabel); break;
                    default: setText (Integer.toString (fpgamode)); break;
                }
                waschecked = (fpgamode != FM_OFF);
                setSelected (waschecked);
            }
        }

        // checkbox clicked
        @Override   // ItemListener
        public void itemStateChanged (ItemEvent e)
        {
            // nop if prompt is open
            if (suppress) return;

            // nop if no change in checkbox
            boolean ischecked = isSelected ();
            if (waschecked != ischecked) {
                waschecked = ischecked;

                // user changed checkbox, prompt to see what to actually do
                JRadioButton butoff  = new JRadioButton ("OFF");
                JRadioButton butreal = new JRadioButton ("REAL");
                JRadioButton butsim  = new JRadioButton ("SIM");
                butoff.setSelected  (fpgamode == FM_OFF);
                butreal.setSelected (fpgamode == FM_REAL);
                butsim.setSelected  (fpgamode == FM_SIM);
                ButtonGroup group = new ButtonGroup ();
                group.add (butoff);
                group.add (butreal);
                group.add (butsim);
                JPanel panel = new JPanel ();
                panel.add (butoff);
                panel.add (butreal);
                panel.add (butsim);
                suppress = true;
                if (JOptionPane.showConfirmDialog (this, panel,
                        "FPGA Mode", JOptionPane.OK_CANCEL_OPTION) == JOptionPane.OK_OPTION) {

                    // update fpgamode pin
                    int newmode = FM_OFF;
                    if (butreal.isSelected ()) newmode = FM_REAL;
                    if (butsim.isSelected  ()) newmode = FM_SIM;
                    GUIZynqPage.pinset (fpgamodepinindex, newmode);
                }
                suppress = false;
                waschecked = isSelected ();
            }
        }
    }

    // memory enable checkbox
    // - manipulates bigmem.v enable bits
    public static class MemCkBox extends JCheckBox implements ItemListener {
        public boolean isenab;
        public int enablopinindex, enabhipinindex;

        public MemCkBox (String label)
        {
            super (label);
            enablopinindex = findpin ("bm_enablo");
            enabhipinindex = findpin ("bm_enabhi");
            addItemListener (this);
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
        @Override   // ItemListener
        public void itemStateChanged (ItemEvent e)
        {
            boolean en = isSelected ();
            if (isenab != en) {
                isenab = en;
                long mask = 0;                                          // assume disabling everything
                if (isenab) {
                    mask = (1L << 62) - 1;                              // assume enabling everything
                    if (fpgamode == FM_REAL) {                          // sim or off, enable everyting
                        GUIZynqPage.pinset (enablopinindex, 0);         // real, turn off all fpga memory
                        GUIZynqPage.pinset (enabhipinindex, 0);
                        for (int page = 0; page < 62; page ++) {        // loop through all possible 4KB pages
                            int rc = GUIZynqPage.rdmem (page << 12);    // see if first word of page readable
                            if (rc != GUIZynqPage.DMAERR_TIMO) mask &= ~ (1L << page);  // if timed out, we need that page
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

    // DZ-11 terminal multiplexor enable checkbox
    public static class DZCkBox extends DevCkBox {

        public DZCkBox (String label)
        {
            super (label, "dz_enable");
        }

        // checkbox clicked
        @Override   // DevCkBox
        public void itemStateChanged (ItemEvent e)
        {
            boolean wasenab = GUIZynqPage.pinget (enabpinindex) > 0;
            super.itemStateChanged (e);
            boolean nowenab = GUIZynqPage.pinget (enabpinindex) > 0;
            if (! wasenab & nowenab) {
                int addrespinindex = findpin ("dz_addres");
                int intvecpinindex = findpin ("dz_intvec");
                GUIZynqPage.pinset (addrespinindex, 0760100);
                GUIZynqPage.pinset (intvecpinindex, 0300);
            }
        }
    }


    // KW-11 line clock enable checkbox
    public static class KWCkBox extends JCheckBox implements ItemListener {
        private boolean suppress;
        private boolean waschecked;
        private boolean wasenabled;
        private boolean wasfiftyhz;
        private int enabledpinindex;
        private int fiftyhzpinindex;

        public KWCkBox (String label)
        {
            super (label);
            enabledpinindex = findpin ("kw_enable");
            fiftyhzpinindex = findpin ("kw_fiftyhz");
            addItemListener (this);
        }

        // called many times a second to make sure checkbox matches fpga
        public void update ()
        {
            boolean isenabled = GUIZynqPage.pinget (enabledpinindex) != 0;
            boolean isfiftyhz = GUIZynqPage.pinget (fiftyhzpinindex) != 0;

            if (wasenabled != isenabled) {
                waschecked = isenabled;
                setSelected (isenabled);
            }

            wasenabled = isenabled;
            wasfiftyhz = isfiftyhz;
        }

        // checkbox clicked
        @Override   // ItemListener
        public void itemStateChanged (ItemEvent e)
        {
            // nop if prompt is open
            if (suppress) return;

            // nop if no change in checkbox
            boolean ischecked = isSelected ();
            if (waschecked != ischecked) {
                waschecked = ischecked;

                // user changed checkbox, prompt to see what to actually do
                JRadioButton butoff = new JRadioButton ("OFF");
                JRadioButton but50  = new JRadioButton ("50Hz");
                JRadioButton but60  = new JRadioButton ("60Hz");
                butoff.setSelected (! wasenabled);
                but50.setSelected  (  wasenabled &&   wasfiftyhz);
                but60.setSelected  (  wasenabled && ! wasfiftyhz);
                ButtonGroup group = new ButtonGroup ();
                group.add (butoff);
                group.add (but50);
                group.add (but60);
                JPanel panel = new JPanel ();
                panel.add (butoff);
                panel.add (but50);
                panel.add (but60);
                suppress = true;
                if (JOptionPane.showConfirmDialog (this, panel,
                        "Line Clock", JOptionPane.OK_CANCEL_OPTION) == JOptionPane.OK_OPTION) {

                    // update kw_enable and kw_fiftyhz pins
                    GUIZynqPage.pinset (enabledpinindex, butoff.isSelected () ? 0 : 1);
                    if (but50.isSelected ()) GUIZynqPage.pinset (fiftyhzpinindex, 1);
                    if (but60.isSelected ()) GUIZynqPage.pinset (fiftyhzpinindex, 0);
                }
                suppress = false;
                waschecked = isSelected ();
            }
        }
    }

    // RL-11 controller enable checkbox
    public static class RLDevCkBox extends DevCkBox {

        public RLDevCkBox (String label)
        {
            super (label, "rl_enable");
        }

        // called several times per second to update the display to match fpga & shm state
        @Override   // DevCkBox
        public void update ()
        {
            // update RL-11 checkbox to match fpga rl_enable
            super.update ();

            // if checkbox changed, update visibility of drives
            int nowvisible = isenab ? 1 : 0;
            if (rldrives[0].isvisible != nowvisible) {
                for (RLDrive rldrive : rldrives) {
                    rldrive.isvisible = nowvisible;
                    rldrive.setVisible (isenab);
                }
                mainframe.pack ();
            }

            // if checked, update the per-drive displays
            if (isenab) {
                for (RLDrive rldrive : rldrives) {
                    rldrive.update ();
                }
            }
        }
    }

    // TM-11 controller enable checkbox
    public static class TMDevCkBox extends DevCkBox {

        public TMDevCkBox (String label)
        {
            super (label, "tm_enable");
        }

        // called several times per second to update the display to match fpga & shm state
        @Override   // DevCkBox
        public void update ()
        {
            // update TM-11 checkbox to match fpga tm_enable
            super.update ();

            // if checkbox changed, update visibility of drives
            int nowvisible = isenab ? 1 : 0;
            if (tmdrives[0].isvisible != nowvisible) {
                for (TMDrive tmdrive : tmdrives) {
                    tmdrive.isvisible = nowvisible;
                    tmdrive.setVisible (isenab);
                }
                mainframe.pack ();
            }

            // if checked, update the per-drive displays
            if (isenab) {
                for (TMDrive tmdrive : tmdrives) {
                    tmdrive.update ();
                }
            }
        }
    }

    // device enable checkbox
    // - plugs/unplugs simulated circuit board from unibus
    public static class DevCkBox extends JCheckBox implements ItemListener {
        public boolean isenab;
        public int enabpinindex;

        public DevCkBox (String label, String enabpin)
        {
            super (label);
            enabpinindex = findpin (enabpin);
            addItemListener (this);
        }

        public void update ()
        {
            boolean sbenab = GUIZynqPage.pinget (enabpinindex) != 0;
            if (isenab != sbenab) {
                isenab = sbenab;
                setSelected (isenab);
            }
        }

        @Override   // ItemListener
        public void itemStateChanged (ItemEvent e)
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
        public boolean rl01shows;
        public int drive;
        public int isvisible;
        public int lastcylno;
        public int lastfnseq;
        public JLabel cylnolbl;
        public JLabel rlmessage;
        public JLabel rloxlbl;
        public long blockmsgupdates;
        public long winkoutready;
        public RLButton loadbutton;
        public RLButton readylight;
        public RLButton faultlight;
        public RLButton wprtswitch;

        public final static int RL01DISKSIZE = 256*2*40*256;  // bytes in RL01 disk file
        public final static int RL02DISKSIZE = 512*2*40*256;  // bytes in RL02 disk file

        public RLDrive (int d)
        {
            drive = d;
            isvisible = -1;
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
            add (rloxlbl    = new JLabel (""));
            add (rlmessage  = new JLabel (""));

            Font lf = new Font ("Monospaced", Font.PLAIN, cylnolbl.getFont ().getSize ());
            cylnolbl.setFont (lf);

            updateLabel ();
            Font rf = rloxlbl.getFont ();
            rloxlbl.setFont (rf.deriveFont (Font.BOLD, rf.getSize () * 2));

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
                        chooser.setFileSelectionMode (JFileChooser.FILES_AND_DIRECTORIES);
                        if (rl01shows) {
                            chooser.setFileFilter (
                                new FileNameExtensionFilter (
                                    "RL01 disk image", "rl01"));
                            chooser.addChoosableFileFilter (
                                new FileNameExtensionFilter (
                                    "RL02 disk image", "rl02"));
                        } else {
                            chooser.setFileFilter (
                                new FileNameExtensionFilter (
                                    "RL02 disk image", "rl02"));
                            chooser.addChoosableFileFilter (
                                new FileNameExtensionFilter (
                                    "RL01 disk image", "rl01"));
                        }
                        chooser.setDialogTitle ("Loading RL drive " + drive);
                        while (true) {
                            if (rlchooserdirectory != null) chooser.setCurrentDirectory (rlchooserdirectory);
                            int rc = chooser.showOpenDialog (RLDrive.this);
                            if (rc != JFileChooser.APPROVE_OPTION) break;
                            File ff = chooser.getSelectedFile ();
                            if (ff.isDirectory ()) {
                                rlchooserdirectory = ff;
                                continue;
                            }
                            rlchooserdirectory = ff.getParentFile ();

                            long issize = ff.length ();
                            long sbsize = 0;
                            String ffname = ff.getName ();
                            if (ffname.toUpperCase ().endsWith (".RL01")) {
                                rl01shows = true;
                                sbsize    = RL01DISKSIZE;
                            }
                            if (ffname.toUpperCase ().endsWith (".RL02")) {
                                rl01shows = false;
                                sbsize    = RL02DISKSIZE;
                            }
                            updateLabel ();
                            if (sbsize == 0) {
                                JOptionPane.showMessageDialog (RLDrive.this, "filename " + ffname + " must end with .rl01 or .rl02",
                                    "Loading Drive", JOptionPane.ERROR_MESSAGE);
                                continue;
                            }
                            if ((issize == sbsize) || (JOptionPane.showConfirmDialog (RLDrive.this,
                                    ffname + " size " + issize + " is not " + sbsize + "\nAre you sure you want to load it?",
                                    "Loading Drive", JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                                String fn = ff.getAbsolutePath ();
                                String er = GUIZynqPage.rlload (drive, wprtswitch.ison, fn);
                                if (er == null) {
                                    rlmessage.setText (fn);
                                    loadbutton.setOn (true);
                                    break;
                                }
                                JOptionPane.showMessageDialog (RLDrive.this, "error loading " + ffname + ": " + er,
                                    "Loading Drive", JOptionPane.ERROR_MESSAGE);
                            }
                        }
                    } else {

                        // unload whatever is in there
                        if ((GUIZynqPage.running () <= 0) ||
                            (JOptionPane.showConfirmDialog (RLDrive.this,
                                    "Are you sure you want to unload drive " + drive + "?",
                                    "Unloading Drive",
                                    JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                            GUIZynqPage.rlload (drive, wprtswitch.ison, "");
                            rlmessage.setText ("");
                            loadbutton.setOn (false);
                        }
                    }
                }
            });

            // do something when WRPRT button is clicked
            wprtswitch.addActionListener (new ActionListener () {
                public void actionPerformed (ActionEvent ae)
                {
                    if (((GUIZynqPage.rlstat (drive) & GUIZynqPage.RLSTAT_LOAD) == 0) ||
                        (GUIZynqPage.running () <= 0) ||
                        (JOptionPane.showConfirmDialog (RLDrive.this,
                                "Are you sure you want to write " + (wprtswitch.ison ? "enable" : "protect") +
                                        " drive " + drive + "?",
                                "Write Protecting Drive",
                                JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                        GUIZynqPage.rlload (drive, ! wprtswitch.ison, null);
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
            if ((stat & GUIZynqPage.RLSTAT_READY) == 0) winkoutready = updatetimemillis + 20;

            // update load, ready, fault, write protect lights
            loadbutton.setOn ((stat & GUIZynqPage.RLSTAT_LOAD)  != 0);
            readylight.setOn (updatetimemillis >= winkoutready);
            faultlight.setOn ((stat & GUIZynqPage.RLSTAT_FAULT) != 0);
            wprtswitch.setOn ((stat & GUIZynqPage.RLSTAT_WRPRT) != 0);

            // update drive type label
            boolean rl01loaded = (stat & GUIZynqPage.RLSTAT_RL01) != 0;
            if (rl01shows != rl01loaded) {
                rl01shows  = rl01loaded;
                updateLabel ();
            }

            // update loaded filename if there was a change and if no error message is being displayed
            int thisfnseq = stat & GUIZynqPage.RLSTAT_FNSEQ;
            if (((blockmsgupdates == 0) && (lastfnseq != thisfnseq)) || (updatetimemillis > blockmsgupdates)) {
                rlmessage.setText (GUIZynqPage.rlfile (drive));
                blockmsgupdates = 0;
                lastfnseq = thisfnseq;
            }
        }

        private void updateLabel ()
        {
            // yes 'O' looks more like real drive logo
            rloxlbl.setText (rl01shows ? " RLO1 " : " RLO2 ");
        }

        @Override
        protected void paintComponent (Graphics g)
        {
            super.paintComponent (g);
            g.drawImage (rl02panimg, 0, 0, null);
        }
    }

    /////////////////
    //  TM DRIVES  //
    /////////////////

    public static File tmchooserdirectory;

    public static class TMButton extends JButton {
        public boolean ison;
        public Color offcolor;
        public Color oncolor;

        public TMButton (String label, Color oncolor, Color offcolor)
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

    public static class TMDrive extends JPanel {
        public int drive;
        public int isvisible;
        public JLabel cylnolbl;
        public JLabel tmmessage;
        public long blockmsgupdates;
        public long lastcylno;
        public long lastfnseq;
        public long winkoutready;
        public TMButton loadbutton;
        public TMButton readylight;
        public TMButton wprtswitch;

        public TMDrive (int d)
        {
            drive = d;
            isvisible = -1;
            lastcylno = -1;
            lastfnseq = -1;

            setLayout (new BoxLayout (this, BoxLayout.X_AXIS));
            setMaximumSize (rl02pandim);
            setMinimumSize (rl02pandim);
            setPreferredSize (rl02pandim);
            setSize (rl02pandim);

            JLabel tmoxlbl;

            add (cylnolbl   = new JLabel ("             "));
            add (loadbutton = new TMButton ("LOAD",  Color.YELLOW, Color.GRAY));
            add (readylight = new TMButton (drive + " RDY", Color.WHITE, Color.GRAY));
            add (wprtswitch = new TMButton ("WRPRT", Color.YELLOW, Color.GRAY));
            add (tmoxlbl    = new JLabel (""));
            add (tmmessage  = new JLabel (""));

            Font lf = new Font ("Monospaced", Font.PLAIN, cylnolbl.getFont ().getSize ());
            cylnolbl.setFont (lf);

            Font rf = tmoxlbl.getFont ();
            tmoxlbl.setFont (rf.deriveFont (Font.BOLD, rf.getSize () * 2));
            tmoxlbl.setText (" TU10 ");

            Dimension md = new Dimension (600, 50);
            tmmessage.setMaximumSize (md);
            tmmessage.setMinimumSize (md);
            tmmessage.setPreferredSize (md);
            tmmessage.setSize (md);

            // do something when LOAD button is clicked
            loadbutton.addActionListener (new ActionListener () {
                public void actionPerformed (ActionEvent ae)
                {
                    if ((GUIZynqPage.tmstat (drive) & GUIZynqPage.TMSTAT_LOAD) == 0) {

                        // display chooser box to select file to load
                        JFileChooser chooser = new JFileChooser ();
                        chooser.setFileSelectionMode (JFileChooser.FILES_AND_DIRECTORIES);
                        chooser.setFileFilter (
                            new FileNameExtensionFilter (
                                "tape image", "tap"));
                        chooser.setDialogTitle ("Loading TM drive " + drive);
                        while (true) {
                            if (tmchooserdirectory != null) chooser.setCurrentDirectory (tmchooserdirectory);
                            int rc = chooser.showOpenDialog (TMDrive.this);
                            if (rc != JFileChooser.APPROVE_OPTION) break;
                            File ff = chooser.getSelectedFile ();
                            if (ff.isDirectory ()) {
                                tmchooserdirectory = ff;
                                continue;
                            }
                            tmchooserdirectory = ff.getParentFile ();

                            long issize = ff.length ();
                            long sbsize = 0;
                            String ffname = ff.getName ();
                            if (! ffname.endsWith (".tap")) {
                                JOptionPane.showMessageDialog (TMDrive.this, "filename " + ffname + " must end with .tap",
                                    "Loading Drive", JOptionPane.ERROR_MESSAGE);
                                continue;
                            }
                            {
                                String fn = ff.getAbsolutePath ();
                                String er = GUIZynqPage.tmload (drive, wprtswitch.ison, fn);
                                if (er == null) {
                                    tmmessage.setText (fn);
                                    loadbutton.setOn (true);
                                    break;
                                }
                                JOptionPane.showMessageDialog (TMDrive.this, "error loading " + ffname + ": " + er,
                                    "Loading Drive", JOptionPane.ERROR_MESSAGE);
                            }
                        }
                    } else {

                        // unload whatever is in there
                        if ((GUIZynqPage.running () <= 0) ||
                            (JOptionPane.showConfirmDialog (TMDrive.this,
                                    "Are you sure you want to unload drive " + drive + "?",
                                    "Unloading Drive",
                                    JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                            GUIZynqPage.tmload (drive, wprtswitch.ison, "");
                            tmmessage.setText ("");
                            loadbutton.setOn (false);
                        }
                    }
                }
            });

            // do something when WRPRT button is clicked
            wprtswitch.addActionListener (new ActionListener () {
                public void actionPerformed (ActionEvent ae)
                {
                    if (((GUIZynqPage.tmstat (drive) & GUIZynqPage.TMSTAT_LOAD) == 0) ||
                        (GUIZynqPage.running () <= 0) ||
                        (JOptionPane.showConfirmDialog (TMDrive.this,
                                "Are you sure you want to write " + (wprtswitch.ison ? "enable" : "protect") +
                                        " drive " + drive + "?",
                                "Write Protecting Drive",
                                JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION)) {
                        GUIZynqPage.tmload (drive, ! wprtswitch.ison, null);
                    }
                }
            });
        }

        // update buttons and text to match fpga & shared memory
        public void update ()
        {
            long stat = GUIZynqPage.tmstat (drive);

            // update cylinder number if it changed, blanks if drive not loaded
            // also wink out the drive ready light
            long thiscylno = ((stat & GUIZynqPage.TMSTAT_LOAD) == 0) ? -1 : (stat & GUIZynqPage.TMSTAT_CYLNO) / (GUIZynqPage.TMSTAT_CYLNO & - GUIZynqPage.TMSTAT_CYLNO);
            if (lastcylno != thiscylno) {
                lastcylno = thiscylno;
                cylnolbl.setText ((lastcylno < 0) ? "             " : String.format ("  %09d  ", lastcylno));
                stat &= ~ GUIZynqPage.TMSTAT_READY;
            }

            // stretch the not-ready status out so it blinks when there is a seek
            if ((stat & GUIZynqPage.TMSTAT_READY) == 0) winkoutready = updatetimemillis + 20;

            // update load, ready, write protect lights
            loadbutton.setOn ((stat & GUIZynqPage.TMSTAT_LOAD)  != 0);
            readylight.setOn (updatetimemillis >= winkoutready);
            wprtswitch.setOn ((stat & GUIZynqPage.TMSTAT_WRPRT) != 0);

            // update loaded filename if there was a change and if no error message is being displayed
            long thisfnseq = stat & GUIZynqPage.TMSTAT_FNSEQ;
            if (((blockmsgupdates == 0) && (lastfnseq != thisfnseq)) || (updatetimemillis > blockmsgupdates)) {
                tmmessage.setText (GUIZynqPage.tmfile (drive));
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
