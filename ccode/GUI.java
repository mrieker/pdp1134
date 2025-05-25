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

// ./GUI -randmem -printinstr
// ./GUI ../silly/doubleroll.oct

// can also do client/server:
//  on zturn:
//   ./GUI -listen 1234
//  on homepc/raspi:
//   ./GUI -connect zturn:1234 -randmem -printinstr

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

    public final static int UPDMS = 100;

    public final static byte CB_STEP   = 1;
    public final static byte CB_HALT   = 2;
    public final static byte CB_CONT   = 3;
    public final static byte CB_SAMPLE = 4;
    public final static byte CB_SETSR  = 5;
    public final static byte CB_RDMEM  = 6;
    public final static byte CB_WRMEM  = 7;
    public final static byte CB_RESET  = 8;

    // access the processor one way or another
    public abstract static class IAccess {
        public int     addr;
        public int     data;
        public int     lreg;
        public int     sreg;
        public boolean running;

        public abstract void sample ();
        public abstract void step ();
        public abstract void cont ();
        public abstract void halt ();
        public abstract void reset ();
        public abstract void setsr (int sr);
        public abstract int rdmem (int addr);
        public abstract int wrmem (int addr, int data);
    }

    // run the GUI with the given processor access
    public static IAccess access;

    public static void main (String[] args)
            throws Exception
    {
        // maybe enter server mode
        //  java GUI -server <port>
        if ((args.length > 1) && args[0].equals ("-listen")) {
            int port = 0;
            try {
                port = Integer.parseInt (args[1]);
            } catch (Exception e) {
                System.err.println ("bad/missing -listen port number");
                System.exit (1);
            }
            if (args.length > 2) {
                System.err.println ("extra parameters after -listen port number");
                System.exit (1);
            }
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
            String[] tcpargs = new String[args.length-2];
            for (i = 0; i < tcpargs.length; i ++) {
                tcpargs[i] = args[i+2];
            }
            access = new TCPAccess (socket, tcpargs);
        }

        // no -connect, access processor directly
        else {
            access = new DirectAccess (args);
        }

        // get initial switch register contents from 777570
        int srint = access.rdmem (0777570);
        writeswitches (srint);

        // set initial display to match processor
        updisplay.actionPerformed (null);
    }

    //////////////
    //  SERVER  //
    //////////////

    // act as TCP server to control the processor
    public static void runServer (int port)
            throws Exception
    {
        // get inbound connection from client
        ServerSocket serversocket = new ServerSocket (port);
        System.out.println ("GUI: listening on port " + port);
        Socket socket = serversocket.accept ();
        System.out.println ("GUI: connection accepted");
        InputStream istream = socket.getInputStream ();
        OutputStream ostream = socket.getOutputStream ();

        // read command line arguments
        int argc = readShort (istream);
        String[] args = new String[argc];
        for (int i = 0; i < argc; i ++) {
            args[i] = readString (istream);
        }

        // get access to zynq fpga page
        access = new DirectAccess (args);

        // read and process incoming command bytes
        byte[] sample = new byte[9];
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
                    sample[2] = (byte)(access.data);
                    sample[3] = (byte)(access.data >>  8);
                    sample[4] = (byte)(access.lreg);
                    sample[5] = (byte)(access.lreg >>  8);
                    sample[6] = (byte)(access.sreg);
                    sample[7] = (byte)(access.sreg >>  8);
                    sample[8] = (byte)(access.running ? 1 : 0);

                    ostream.write (sample, 0, 7);
                    break;
                }
                case CB_SETSR: {
                    int sr = readShort (istream);
                    access.setsr (sr);
                    break;
                }
                case CB_RDMEM: {
                    int addr = readShort (istream);
                    int rc = access.rdmem (addr);
                    sample[0] = (byte)(rc);
                    sample[1] = (byte)(rc >>  8);
                    sample[2] = (byte)(rc >> 16);
                    sample[3] = (byte)(rc >> 24);
                    ostream.write (sample, 0, 4);
                    break;
                }
                case CB_WRMEM: {
                    int addr = readShort (istream);
                    int data = readShort (istream);
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
                default: {
                    throw new Exception ("bad command byte received " + cmdbyte);
                }
            }
        }
        socket.close ();
    }

    // read string from client
    // - two-byte little endian bytecount
    // - byte string
    public static String readString (InputStream istream)
            throws Exception
    {
        int len = readShort (istream);
        byte[] bytes = new byte[len];
        for (int i = 0; i < len;) {
            int rc = istream.read (bytes, i, len - i);
            if (rc <= 0) throw new EOFException ("EOF reading network");
            i += rc;
        }
        return new String (bytes);
    }

    // read short from client
    // - little endian
    public static int readShort (InputStream istream)
            throws Exception
    {
        int value = istream.read ();
        value |= istream.read () << 8;
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

        public TCPAccess (Socket socket, String[] args)
            throws Exception
        {
            istream = socket.getInputStream ();
            ostream = socket.getOutputStream ();

            byte[] shortbytes = new byte[2];
            shortbytes[0] = (byte)args.length;
            shortbytes[1] = (byte)(args.length >> 8);
            ostream.write (shortbytes);
            for (String arg : args) {
                byte[] argbytes = arg.getBytes ();
                shortbytes[0] = (byte)argbytes.length;
                shortbytes[1] = (byte)(argbytes.length >> 8);
                ostream.write (shortbytes);
                ostream.write (argbytes);
            }
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
        public void sample ()
        {
            try {
                ostream.write (CB_SAMPLE);
                for (int i = 0; i < 9;) {
                    int rc = istream.read (samplebytes, i, 9 - i);
                    if (rc <= 0) throw new EOFException ("eof reading network");
                    i += rc;
                }
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }

            addr = ((samplebytes[1] & 0xFF) << 8) | (samplebytes[0] & 0xFF);
            data = ((samplebytes[3] & 0xFF) << 8) | (samplebytes[2] & 0xFF);
            lreg = ((samplebytes[5] & 0xFF) << 8) | (samplebytes[4] & 0xFF);
            sreg = ((samplebytes[7] & 0xFF) << 8) | (samplebytes[6] & 0xFF);
            running = samplebytes[8] != 0;
        }

        @Override
        public void setsr (int sr)
        {
            byte[] msg = { CB_SETSR, (byte) sr, (byte) (sr >> 8) };
            try {
                ostream.write (msg);
            } catch (Exception e) {
                e.printStackTrace ();
                System.exit (1);
            }
        }

        @Override
        public int rdmem (int addr)
        {
            byte[] msg = { CB_RDMEM, (byte) addr, (byte) (addr >> 8) };
            byte[] reply = new byte[4];
            try {
                ostream.write (msg);
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
            byte[] msg = { CB_WRMEM, (byte) addr, (byte) (addr >> 8), (byte) data, (byte) (data >> 8) };
            byte[] reply = new byte[4];
            try {
                ostream.write (msg);
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

        public DirectAccess (String[] args)
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
        public void sample ()
        {
            addr = GUIZynqPage.addr ();
            data = GUIZynqPage.data ();
            lreg = GUIZynqPage.lreg ();
            sreg = GUIZynqPage.sreg ();
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

    // update display with processor state
    public final static ActionListener updisplay =
        new ActionListener () {
            @Override
            public void actionPerformed (ActionEvent ae)
            {
                // read values from zynq fpga (either directly or via tcp)
                access.sample ();
                int     addr = access.addr;
                int     data = access.data;
                int     lreg = access.lreg;
                boolean running = access.running;

                // update display LEDs
                writeaddrleds (addr);
                writedataleds (data);
                writelregleds (lreg);
                runled.setOn  (running);
                berrled.setOn (false);
            }
        };

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
        bits0806.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel (""));
        bits0503.add (centeredLabel ("A"));
        bits0200.add (centeredLabel ("D"));
        bits0200.add (centeredLabel ("D"));
        bits0200.add (centeredLabel ("R"));

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

        // row 4 - 777570 label
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1715.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel (""));
        bits1412.add (centeredLabel ("7"));
        bits1109.add (centeredLabel ("7"));
        bits1109.add (centeredLabel ("7"));
        bits1109.add (centeredLabel ("5"));
        bits0806.add (centeredLabel ("7"));
        bits0806.add (centeredLabel ("0"));
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

        // row 6 - blank
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
        bits0503.add (centeredLabel (""));
        bits0200.add (centeredLabel (""));
        bits0200.add (centeredLabel (""));
        bits0200.add (centeredLabel (""));

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
            Color ledcolor = ison ? Color.RED : Color.BLACK;
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
            access.setsr (readswitches () & 077777);
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
            Color ledcolor = ison ? Color.RED : Color.BLACK;
            g.setColor (ledcolor);
            g.fillArc (P, P, D, D, 0, 360);
        }
    }

    public static class StepButton extends JButton implements ActionListener {
        public StepButton ()
        {
            super ("STEP");
            addActionListener (this);
        }

        @Override  // ActionListener
        public void actionPerformed (ActionEvent ae)
        {
            access.step ();
            updisplay.actionPerformed (null);
        }
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
            autoincloadedaddress = 0;
            loadedaddress = readswitches () & 0777777;
            writeaddrleds (loadedaddress);
            writedataleds (0);
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
            if (autoincloadedaddress < 0) {
                int inc = ((loadedaddress >= 0777700) && (loadedaddress <= 0777717)) ? 1 : 2;
                loadedaddress = (loadedaddress + inc) & 0777777;
            }
            writeaddrleds (loadedaddress);
            int rc = access.rdmem (loadedaddress);
            berrled.setOn (rc < 0);
            writedataleds (rc & 0177777);
            autoincloadedaddress = -1;
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
            access.reset ();
            updisplay.actionPerformed (null);
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
            access.reset ();
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
            access.reset ();
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
            access.reset ();
            access.wrmem (0777707, loadedaddress);
            access.wrmem (0777776, 0340);
            access.cont  ();
            updisplay.actionPerformed (null);
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
