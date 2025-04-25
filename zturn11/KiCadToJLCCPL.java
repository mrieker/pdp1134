
// convert KiCad all-position to JLCPCB component placement format

// java KiCadToJLCCPL < zturn36/gerber/zturn36-all-pos.csv > zturn36/gerber/zturn36-jlc-cpl.csv

// KiCad produces Ref,Val,Package,PosX,PosY,Rot,Side
// JLCPCB accepts Designator,Mid X,Mid Y,Layer,Rotation

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;

public class KiCadToJLCCPL {
    public static void main (String[] args)
            throws Exception
    {
        BufferedReader br = new BufferedReader (new InputStreamReader (System.in));

        String line = br.readLine ();
        if (! line.equals ("Ref,Val,Package,PosX,PosY,Rot,Side")) {
            throw new Exception ("not known format");
        }
        System.out.println ("Designator,Mid X,Mid Y,Layer,Rotation");

        while ((line = br.readLine ()) != null) {
            String[] cols = splitcsv (line);
            double xx =   Double.parseDouble (cols[3]);
            double yy = - Double.parseDouble (cols[4]);
            String layer = cols[6];
            if (layer.equals ("bottom")) layer = "Bottom";
            if (layer.equals ("top"))    layer = "Top";
            long rot = Math.round (Double.parseDouble (cols[5]));
            System.out.println (cols[0] + "," + xx + "mm," + yy + "mm," + layer + "," + rot);
        }
    }

    private static final String[] emptystringarray = new String[0];

    public static String[] splitcsv (String line)
    {
        ArrayList<String> strings = new ArrayList<> ();
        boolean quoted = false;
        StringBuilder sb = new StringBuilder ();
        for (int i = 0; i < line.length (); i ++) {
            char c = line.charAt (i);
            if (! quoted && (c == ',')) {
                strings.add (sb.toString ());
                sb = new StringBuilder ();
                continue;
            }
            if (c == '"') {
                quoted = ! quoted;
            }
            sb.append (c);
        }
        strings.add (sb.toString ());
        return strings.toArray (emptystringarray);
    }
}
