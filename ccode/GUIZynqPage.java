
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

    public native static int rdmem (int addr);
    public native static int wrmem (int addr, int data);
}
