
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

    public final static int RLSTAT_LOAD  = 000000001;
    public final static int RLSTAT_WRPRT = 000000002;
    public final static int RLSTAT_READY = 000000004;
    public final static int RLSTAT_FAULT = 000000010;
    public final static int RLSTAT_FNSEQ = 000007760;
    public final static int RLSTAT_CYLNO = 007770000;
    public final static int RLSTAT_RL01  = 010000000;

    public native static String rlload (int drive, boolean readonly, String filename);
    public native static int    rlstat (int drive);
    public native static String rlfile (int drive);
    public native static int    rlfast (int newflag);

    public final static long TMSTAT_LOAD  = 0000000000000001L;
    public final static long TMSTAT_WRPRT = 0000000000000002L;
    public final static long TMSTAT_READY = 0000000000000004L;
    public final static long TMSTAT_FAULT = 0000000000000010L;
    public final static long TMSTAT_FNSEQ = 0000000000007760L;
    public final static long TMSTAT_CYLNO = 0377777777770000L;

    public native static String tmload (int drive, boolean readonly, String filename);
    public native static long   tmstat (int drive);
    public native static String tmfile (int drive);
    public native static int    tmfast (int newflag);
}
