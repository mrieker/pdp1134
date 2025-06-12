
// interface between GUI.java and Zynq access page

// javac -h . GUIZynqPage.java

public class GUIZynqPage {

    static {
        try {
            String unamem = System.getProperty ("unamem");  // java -Dunamem=`uname -m`
            System.loadLibrary ("GUIZynqPage." + unamem);   // looks for libGUIZynqPage.`uname -m`.so
        } catch (Throwable t) {
            t.printStackTrace (System.err);
            System.exit (1);
        }
    }

    public native static int open ();
    public native static int step ();
    public native static int cont ();
    public native static int halt ();
    public native static int reset ();

    public native static int addr ();
    public native static int data ();
    public native static int getlr ();
    public native static int getsr ();
    public native static int running ();
    public native static void setsr (int sr);

    public final static int DMAERR_TIMO = -1;   // timed out, nothing at that address
    public final static int DMAERR_PARE = -2;   // parity error, something there but corrupt
    public final static int DMAERR_STUK = -3;   // stuck, being blocked by real KY-11

    public native static int rdmem (int addr);
    public native static int wrmem (int addr, int data);

    public native static int pinfind (String name);
    public native static int pinget (int index);
    public native static boolean pinset (int index, int value);

    public final static int RLSTAT_LOAD  = 00000001;
    public final static int RLSTAT_WRPRT = 00000002;
    public final static int RLSTAT_READY = 00000004;
    public final static int RLSTAT_FAULT = 00000010;
    public final static int RLSTAT_FNSEQ = 00007760;
    public final static int RLSTAT_CYLNO = 07770000;

    public native static String rlload (int drive, boolean readonly, String filename);
    public native static int    rlstat (int drive);
    public native static String rlfile (int drive);
}
