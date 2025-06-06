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

// can also do client/server:
//  on zturn:
//   ./GUI -listen 1234
//  on homepc/raspi:
//   ./GUI -connect zturn:1234

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
import java.io.EOFException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.SwingUtilities;
import javax.swing.Timer;

public class GUI extends JPanel {

    public final static Color ledoncolor = Color.CYAN;

    public final static int UPDMS = 3;

    public final static byte CB_STEP   = 1;
    public final static byte CB_HALT   = 2;
    public final static byte CB_CONT   = 3;
    public final static byte CB_SAMPLE = 4;
    public final static byte CB_SETSR  = 5;
    public final static byte CB_RDMEM  = 6;
    public final static byte CB_WRMEM  = 7;
    public final static byte CB_RESET  = 8;
    public final static byte CB_CHKHLT = 9;

    // access the processor one way or another
    public abstract static class IAccess {
        public int addr;
        public int data;
        public int lreg;
        public int sreg;
        public int running;

        public abstract void sample ();
        public abstract void step ();
        public abstract void cont ();
        public abstract void halt ();
        public abstract void reset ();
        public abstract boolean chkhlt ();
        public abstract void setsr (int sr);
        public abstract int rdmem (int addr);
        public abstract int wrmem (int addr, int data);
    }

    // run the GUI with the given processor access
    public static IAccess access;

    public static void main (String[] args)
    {
        try {

            // maybe enter server mode
            //  java GUI -listen <port>
            if ((args.length > 1) && args[0].equals ("-listen")) {
                int port = 0;
                try {
                    port = Integer.parseInt (args[1]);
                } catch (Exception e) {
                    System.err.println ("bad/missing -listen port number");
                    System.exit (1);
                }

                // open access to the Zynq
                String[] args2 = new String[args.length-2];
                for (int i = 0; i < args2.length; i ++) args2[i] = args[i+2];
                openAccess (args2);

                // forward from incoming circuit to Zynq and wisa-wersa
                runServer (port);
                System.exit (0);
            }

            // not server, create window and show it
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
                }
            });

            // open access to Zynq board
            openAccess (args);

            // get initial switch register contents from 777570
            int srint = access.rdmem (0777570);
            writeswitches (srint);

            // set initial display to match processor
            updisplay.actionPerformed (null);
        } catch (Exception e) {
            e.printStackTrace ();
            System.exit (1);
        }
    }

    // set up access to the zynq board
    // either direct (we're running on the zynq)
    // ...or remote (we connecting to zynq via tcp connection)
    public static void openAccess (String[] args)
            throws Exception
    {
        // if -connect, use TCP to connect to server and access the processor that way
        if ((args.length > 1) && args[0].equals ("-connect")) {
            String host = "localhost";
            int i = args[1].indexOf (':');
            String portstr = args[1];
            if (i >= 0) {
                host = portstr.substring (0, i);
                portstr = portstr.substring (++ i);
            }
            int port = 0;
            try {
                port = Integer.parseInt (portstr);
            } catch (Exception e) {
                System.err.println ("bad port number " + portstr);
                System.exit (1);
            }
            Socket socket = new Socket (host, port);
            access = new TCPAccess (socket);
        }

        else if (args.length > 0) {
            System.err.println ("unknown argument/option " + args[0]);
            System.exit (1);
        }

        // no -connect, access processor directly
        else {
            access = new DirectAccess ();
        }
    }

    //////////////
    //  SERVER  //
    //////////////

    // act as TCP server to control the processor
    public static void runServer (int port)
            throws Exception
    {
        ServerSocket serversocket = new ServerSocket (port);

        while (true) {
            Socket socket = null;
            try {

                // get inbound connection from client
                System.out.println ("GUI: listening on port " + port);
                socket = serversocket.accept ();
                System.out.println ("GUI: connection accepted");
                InputStream istream = socket.getInputStream ();
                OutputStream ostream = socket.getOutputStream ();

                // read and process incoming command bytes
                byte[] sample = new byte[10];
                for (int cmdbyte; (cmdbyte = istream.read ()) >= 0;) {
                    switch (cmdbyte) {
                        case CB_STEP: {
                            access.step ();
                            break;
                        }
                        case CB_HALT: {
                            access.halt ();
                            break;
                        }
                        case CB_CONT: {
                            access.cont ();
                            break;
                        }
                        case CB_SAMPLE: {
                            access.sample ();

                            sample[0] = (byte)(access.addr);
                            sample[1] = (byte)(access.addr >>  8);
                            sample[2] = (byte)(access.addr >>  16);
                            sample[3] = (byte)(access.data);
                            sample[4] = (byte)(access.data >>  8);
                            sample[5] = (byte)(access.lreg);
                            sample[6] = (byte)(access.lreg >>  8);
                            sample[7] = (byte)(access.sreg);
                            sample[8] = (byte)(access.sreg >>  8);
                            sample[9] = (byte)(access.running);

                            ostream.write (sample, 0, 10);
                            break;
                        }
                        case CB_SETSR: {
                            int sr = read24 (istream);
                            access.setsr (sr);
                            break;
                        }
                        case CB_RDMEM: {
                            int addr = read24 (istream);
                            int rc = access.rdmem (addr);
                            sample[0] = (byte)(rc);
                            sample[1] = (byte)(rc >>  8);
                            sample[2] = (byte)(rc >> 16);
                            sample[3] = (byte)(rc >> 24);
                            ostream.write (sample, 0, 4);
                            break;
                        }
                        case CB_WRMEM: {
                            int addr = read24 (istream);
                            int data = read16 (istream);
                            int rc = access.wrmem (addr, data);
                            sample[0] = (byte)(rc);
                            sample[1] = (byte)(rc >>  8);
                            sample[2] = (byte)(rc >> 16);
                            sample[3] = (byte)(rc >> 24);
                            ostream.write (sample, 0, 4);
                            break;
                        }
                        case CB_RESET: {
                            access.reset ();
                            break;
                        }
                        case CB_CHKHLT: {
                            boolean ok = access.chkhlt ();
                            ostream.write ((byte) (ok ? 1 : 0));
                            break;
                        }
                        default: {
                            throw new Exception ("bad command byte received " + cmdbyte);
                        }
                    }
                }
            } catch (Exception e) {
                e.printStackTrace ();
            }
            try {
                socket.close ();
            } catch (Exception e) { }
        }
    }

    // read string from client
    // - two-byte little endian bytecount
    // - byte string
    public static String readString (InputStream istream)
            throws Exception
    {
        int len = read16 (istream);
        byte[] bytes = new byte[len];
        for (int i = 0; i < len;) {
            int rc = istream.read (bytes, i, len - i);
            if (rc <= 0) throw new EOFException ("EOF reading network");
            i += rc;
        }
        return new String (bytes);
    }

    // read 16-bit integer from client
    // - little endian
    public static int read16 (InputStream istream)
            throws Exception
    {
        int value = istream.read ();
        value |= istream.read () << 8;
        if (value < 0) throw new EOFException ("EOF reading network");
        return value;
    }

    // read 24-bit integer from client
    // - little endian
    public static int read24 (InputStream istream)
            throws Exception
    {
        int value = istream.read ();
        value |= istream.read () <<  8;
        value |= istream.read () << 16;
        if (value < 0) throw new EOFException ("EOF reading network");
        return value;
    }

    //////////////
    //  CLIENT  //
    //////////////

    // access the processor via TCP connection
    public static class TCPAccess extends IAccess {
        public InputStream istream;
        public OutputStream ostream;

        public byte[] samplebytes = new byte[25];

        public TCPAccess (Socket socket)
            throws Exception
        {
            istream = socket.getInputStream ();
            ostream = socket.getOutputStream ();
        }

        @Override
        public void step ()
        {
            try {
                ostream.write (CB_STEP);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public void cont ()
        {
            try {
                ostream.write (CB_CONT);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public void halt ()
        {
            try {
                ostream.write (CB_HALT);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public void reset ()
        {
            try {
                ostream.write (CB_RESET);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public boolean chkhlt ()
        {
            int rc = 0;
            try {
                ostream.write (CB_CHKHLT);
                rc = istream.read ();
                if (rc < 0) throw new EOFException ("eof reading network");
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
            return rc > 0;
        }

        @Override
        public void sample ()
        {
            try {
                ostream.write (CB_SAMPLE);
                for (int i = 0; i < 9;) {
                    int rc = istream.read (samplebytes, i, 10 - i);
                    if (rc <= 0) throw new EOFException ("eof reading network");
                    i += rc;
                }
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }

            addr = ((samplebytes[2] & 0xFF) << 16) | ((samplebytes[1] & 0xFF) << 8) | (samplebytes[0] & 0xFF);
            data =                                   ((samplebytes[4] & 0xFF) << 8) | (samplebytes[3] & 0xFF);
            lreg =                                   ((samplebytes[6] & 0xFF) << 8) | (samplebytes[5] & 0xFF);
            sreg =                                   ((samplebytes[8] & 0xFF) << 8) | (samplebytes[7] & 0xFF);
            running = samplebytes[9];
        }

        @Override
        public void setsr (int sr)
        {
            samplebytes[0] = CB_SETSR;
            samplebytes[1] = (byte) sr;
            samplebytes[2] = (byte) (sr >> 8);
            samplebytes[3] = (byte) (sr >> 16);
            try {
                ostream.write (samplebytes, 0, 4);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public int rdmem (int addr)
        {
            samplebytes[0] = CB_RDMEM;
            samplebytes[1] = (byte) addr;
            samplebytes[2] = (byte) (addr >> 8);
            samplebytes[3] = (byte) (addr >> 16);
            try {
                ostream.write (samplebytes, 0, 4);
                for (int i = 0; i < 4;) {
                    int rc = istream.read (samplebytes, i, 4 - i);
                    if (rc <= 0) throw new EOFException ("eof reading network");
                    i += rc;
                }
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
            return ((samplebytes[3] & 0xFF) << 24) | ((samplebytes[2] & 0xFF) << 16) | ((samplebytes[1] & 0xFF) << 8) | (samplebytes[0] & 0xFF);
        }

        @Override
        public int wrmem (int addr, int data)
        {
            samplebytes[0] = CB_WRMEM;
            samplebytes[1] = (byte) addr;
            samplebytes[2] = (byte) (addr >> 8);
            samplebytes[3] = (byte) (addr >> 16);
            samplebytes[4] = (byte) data;
            samplebytes[5] = (byte) (data >> 8);
            try {
                ostream.write (samplebytes, 0, 6);
                for (int i = 0; i < 4;) {
                    int rc = istream.read (samplebytes, i, 4 - i);
                    if (rc <= 0) throw new EOFException ("eof reading network");
                    i += rc;
                }
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
            return ((samplebytes[3] & 0xFF) << 24) | ((samplebytes[2] & 0xFF) << 16) | ((samplebytes[1] & 0xFF) << 8) | (samplebytes[0] & 0xFF);
        }
    }

    /////////////////////
    //  DIRECT ACCESS  //
    /////////////////////

    // access the zynq fpga page via mmap()
    public static class DirectAccess extends IAccess {

        public DirectAccess ()
        {
            GUIZynqPage.open ();
        }

        @Override
        public void step ()
        {
            GUIZynqPage.step ();
        }

        @Override
        public void cont ()
        {
            GUIZynqPage.cont ();
        }

        @Override
        public void halt ()
        {
            GUIZynqPage.halt ();
        }

        @Override
        public void reset ()
        {
            GUIZynqPage.reset ();
        }

        @Override
        public boolean chkhlt ()
        {
            for (int i = 0; GUIZynqPage.running () > 0; i ++) {
                if (i > 100000) {
                    return false;
                }
            }
            return true;
        }

        @Override
        public void sample ()
        {
            addr = GUIZynqPage.addr ();
            data = GUIZynqPage.data ();
            lreg = GUIZynqPage.getlr ();
            sreg = GUIZynqPage.getsr ();
            running = GUIZynqPage.running ();
        }

        @Override
        public void setsr (int sr)
        {
            GUIZynqPage.setsr (sr);
        }

        @Override
        public int rdmem (int addr)
        {
            return GUIZynqPage.rdmem (addr);
        }

        @Override
        public int wrmem (int addr, int data)
        {
            return GUIZynqPage.wrmem (addr, data);
        }
    }

    ////////////////////
    //  GUI ELEMENTS  //
    ////////////////////

    public static long lastcc = -1;

    public static LED gpioiakled;
    public static LED gpiowrled;
    public static LED gpiordled;
    public static LED gpiodfled;
    public static LED gpioioled;
    public static LED gpiojmpled;
    public static LED gpiodenled;
    public static LED gpioirqled;
    public static LED gpioqenled;
    public static LED gpioiosled;
    public static LED gpioresled;
    public static LED gpioclkled;
    public static LED[] mbleds = new LED[13];

    public static LED[] addrleds = new LED[18];
    public static LED[] dataleds = new LED[16];
    public static LED[] lregleds = new LED[16];
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

    public static JLabel messagelabel;

    // update display with processor state
    public final static ActionListener updisplay =
        new ActionListener () {
            @Override
            public void actionPerformed (ActionEvent ae)
            {
                // read values from zynq fpga (either directly or via tcp)
                access.sample ();

                // update display LEDs
                writeaddrleds (access.addr);
                writedataleds (access.data);
                writelregleds (access.lreg);
                runled.setOn  (access.running > 0);
                berrled.setOn (false);

                // update grayed buttons based on running state
                //  running = +1 : processor is running
                //             0 : processor halted but is resumable
                //            -1 : processor halted, reset required
                if (lastrunning != access.running) {
                    lastrunning = access.running;
                    ldadbutton.setEnabled  (lastrunning <= 0);
                    exambutton.setEnabled  (lastrunning <= 0);
                    depbutton.setEnabled   (lastrunning <= 0);
                    haltbutton.setEnabled  (lastrunning  > 0);
                    stepbutton.setEnabled  (lastrunning == 0);
                    contbutton.setEnabled  (lastrunning == 0);
                    startbutton.setEnabled (lastrunning <= 0);
                    resetbutton.setEnabled (true);
                }

                // if halted, display PC,PS
                if (access.running <= 0) {
                    displaypcps ();
                }

                // start automatic updates while running
                if ((access.running > 0) && (runupdatimer == null)) {
                    runupdatimer = new Timer (UPDMS, updisplay);
                    runupdatimer.start ();
                }

                // stop automatic updates while halted so we don't munch addr & data leds, etc
                if ((access.running <= 0) && (runupdatimer != null)) {
                    runupdatimer.stop ();
                    runupdatimer = null;
                }
            }
        };

    public static int lastrunning = 12345;
    public static Timer runupdatimer;

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
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel ("A"));
        bits0503.add (centeredLabel ("D"));
        bits0503.add (centeredLabel ("D"));
        bits0503.add (centeredLabel ("R"));
        bits0200.add (centeredLabel ("E"));
        bits0200.add (centeredLabel ("S"));
        bits0200.add (centeredLabel ("S"));

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
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel ("D"));
        bits0200.add (centeredLabel ("A"));
        bits0200.add (centeredLabel ("T"));
        bits0200.add (centeredLabel ("A"));

        // row 3 - data bits
        bits1715.add (centeredLabel (""));
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
        bits0806.add (centeredLabel (""));
        bits0806.add (centeredLabel (""));
        bits0503.add (centeredLabel ("L"));
        bits0503.add (centeredLabel ("I"));
        bits0503.add (centeredLabel ("G"));
        bits0200.add (centeredLabel ("H"));
        bits0200.add (centeredLabel ("T"));
        bits0200.add (centeredLabel ("S"));

        // row 5 - 777570 lights
        bits1715.add (centeredLabel (""));
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

        buttonbox1.add (runled = new LED ());
        buttonbox1.add (berrled = new LED ());

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
    public static int readswitches ()
    {
        int sr = 0;
        for (int i = 0; i < 18; i ++) {
            Switch sw = switches[i];
            if (sw.ison) sr |= 1 << i;
        }
        return sr;
    }

    // write 18-bit value to SR (switch register) leds
    public static void writeswitches (int sr)
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

    public static class LED extends JPanel {
        public final static int P = 5;  // padding
        public final static int D = 20; // diameter

        public boolean ison;

        public LED ()
        {
            Dimension d = new Dimension (P + D + P, P + D + P);
            setMaximumSize (d);
            setMinimumSize (d);
            setPreferredSize (d);
            setSize (d);
            repaint ();
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                repaint ();
            }
        }

        @Override
        public void paint (Graphics g)
        {
            g.setColor (Color.GRAY);
            g.fillArc (P - 3, P - 3, D + 6, D + 6, 0, 360);
            Color ledcolor = ison ? ledoncolor : Color.BLACK;
            g.setColor (ledcolor);
            g.fillArc (P, P, D, D, 0, 360);
        }
    }

    public static class Switch extends JButton implements ActionListener {
        public final static int P = 5;
        public final static int D = 20;

        public boolean ison;

        public Switch ()
        {
            Dimension d = new Dimension (P + D + P, P + D + P);
            setMaximumSize (d);
            setMinimumSize (d);
            setPreferredSize (d);
            setSize (d);
            addActionListener (this);
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            setOn (! ison);
            access.setsr (readswitches () & 0177777);
        }

        public void setOn (boolean on)
        {
            if (ison != on) {
                ison = on;
                repaint ();
            }
        }

        @Override
        public void paint (Graphics g)
        {
            g.setColor (Color.GRAY);
            g.fillArc (P - 3, P - 3, D + 6, D + 6, 0, 360);
            Color ledcolor = ison ? ledoncolor : Color.BLACK;
            g.setColor (ledcolor);
            g.fillArc (P, P, D, D, 0, 360);
        }
    }

    ///////////////////////////////
    // PROCESSOR CONTROL BUTTONS //
    ///////////////////////////////

    public static class StepButton extends JButton implements ActionListener {
        public StepButton ()
        {
            super ("STEP");
            addActionListener (this);
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("stepping processor");
            access.step ();                     // tell processor to step single instruction
            checkforhalt ();                    // make sure it halted
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
            messagelabel.setText ("halting processor");
            access.halt ();                     // tell processor to stop executing instructions
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
            messagelabel.setText ("resuming processor");
            access.cont ();
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
            messagelabel.setText ("resetting processor");
            access.reset ();
            checkforhalt ();
            messagelabel.setText ("processor reset complete");
            updisplay.actionPerformed (null);
        }
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
            messagelabel.setText ("resetting processor");
            access.reset ();
            if (checkforhalt ()) {
                messagelabel.setText ("writing PC and PS");
                if (access.wrmem (0777707, loadedaddress) < 0) {
                    messagelabel.setText ("writing PC failed");
                } else if (access.wrmem (0777776, 0340) < 0) {
                    messagelabel.setText ("writing PS failed");
                } else {
                    messagelabel.setText (String.format ("starting at %06o", loadedaddress));
                    access.cont  ();
                }
            }
            updisplay.actionPerformed (null);
        }
    }

    // halt was requested, wait here for processor to actually halt
    public static boolean checkforhalt ()
    {
        boolean ok = access.chkhlt ();
        if (! ok) messagelabel.setText ("processor failed to halt");
        return ok;
    }

    // processor just halted, display PC and PS
    public static void displaypcps ()
    {
        int pc = access.rdmem (0777707);
        String text = "";
        if (pc >= 0) {
            loadedaddress = pc;
            autoincloadedaddress = 0;
            writeaddrleds (pc);
            text += String.format ("stopped at PC %06o", pc);
        } else {
            text += "stopped at PC unknown";
        }
        int ps = access.rdmem (0777776);
        if (ps >= 0) {
            text += String.format (", PS %06o", ps);
        } else {
            text += ", PS unknown";
        }
        messagelabel.setText (text);
    }

    ///////////////////////////
    // MEMORY ACCESS BUTTONS //
    ///////////////////////////

    public static int loadedaddress;
    public static int autoincloadedaddress;

    // - load address button
    public static class LdAdButton extends MemButton {
        public LdAdButton ()
        {
            super ("LDAD");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("");
            autoincloadedaddress = 0;
            loadedaddress = readswitches () & 0777777;
            if ((loadedaddress < 0777700) || (loadedaddress > 0777717)) {
                loadedaddress &= 0777776;
            }
            writeaddrleds (loadedaddress);
            writedataleds (0);
            messagelabel.setText (String.format ("loaded address %06o", loadedaddress));
        }
    }

    // - examine button
    public static class ExamButton extends MemButton {
        public ExamButton ()
        {
            super ("EXAM");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("");
            if (autoincloadedaddress < 0) {
                int inc = ((loadedaddress >= 0777700) && (loadedaddress <= 0777717)) ? 1 : 2;
                loadedaddress = (loadedaddress + inc) & 0777777;
            }
            writeaddrleds (loadedaddress);
            int rc = access.rdmem (loadedaddress);
            berrled.setOn (rc < 0);
            writedataleds (rc & 0177777);
            autoincloadedaddress = -1;
            if (rc < 0) messagelabel.setText (String.format ("examined address %06o, bus timed out", loadedaddress));
            else messagelabel.setText (String.format ("examined address %06o, data %06o", loadedaddress, rc & 0177777));
        }
    }

    // - deposit button
    public static class DepButton extends MemButton {
        public DepButton ()
        {
            super ("DEP");
        }

        @Override  // MemButton
        public void actionPerformed (ActionEvent ae)
        {
            messagelabel.setText ("");
            if (autoincloadedaddress > 0) {
                int inc = ((loadedaddress >= 0777700) && (loadedaddress <= 0777717)) ? 1 : 2;
                loadedaddress = (loadedaddress + inc) & 0777777;
            }
            writeaddrleds (loadedaddress);
            int data = readswitches () & 0177777;
            int rc = access.wrmem (loadedaddress, data);
            berrled.setOn (rc < 0);
            writedataleds (data);
            autoincloadedaddress = 1;
            if (rc < 0) messagelabel.setText (String.format ("deposited to address %06o, bus timed out", loadedaddress));
            else messagelabel.setText (String.format ("deposited to address %06o, data %06o", loadedaddress, data));
        }
    }

    public static abstract class MemButton extends JButton implements ActionListener {
        public MemButton (String lbl)
        {
            super (lbl);
            addActionListener (this);
        }

        @Override  // ActionListener
        public abstract void actionPerformed (ActionEvent ae);
    }
}
