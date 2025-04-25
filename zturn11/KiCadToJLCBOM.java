
// convert KiCad to JLCPCB bill-of-materials format

// java KiCadToJLCBOM < zturn36/gerber/zturn36-bom.csv > zturn36/gerber/zturn36-jlc-bom.csv

// KiCad produces "Id";"Designator";"Footprint";"Quantity";"Designation";"Supplier and ref";
// JLCPCB accepts Comment,Designator,Footprint,JLCPCB Part #

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;

public class KiCadToJLCBOM {
    public static void main (String[] args)
            throws Exception
    {
        BufferedReader br = new BufferedReader (new InputStreamReader (System.in));

        String line = br.readLine ();
        if (! line.equals ("\"Id\";\"Designator\";\"Footprint\";\"Quantity\";\"Designation\";\"Supplier and ref\";")) {
            throw new Exception ("not known format");
        }
        System.out.println ("Comment,Designator,Footprint,JLCPCB Part #");

        while ((line = br.readLine ()) != null) {
            String[] cols = splitcsv (line);
            String desig = cols[1];
            String footp = cols[2];
            String comnt = cols[4];
            System.out.println (comnt + "," + desig + "," + footp + ",");
        }
    }

    private static final String[] emptystringarray = new String[0];

    public static String[] splitcsv (String line)
    {
        ArrayList<String> strings = new ArrayList<> ();
        boolean needsquotes = false;
        boolean quoted = false;
        StringBuilder sb = new StringBuilder ();
        for (int i = 0; i < line.length (); i ++) {
            char c = line.charAt (i);
            if (! quoted && (c == ';')) {
                if (needsquotes) sb.insert (0, "\"");
                if (needsquotes) sb.append ("\"");
                strings.add (sb.toString ());
                needsquotes = false;
                sb = new StringBuilder ();
                continue;
            }
            if (c == '"') {
                needsquotes = true;
                quoted = ! quoted;
                continue;
            }
            if (c == ',') needsquotes = true;
            sb.append (c);
        }
        if (needsquotes) sb.insert (0, "\"");
        if (needsquotes) sb.append ("\"");
        strings.add (sb.toString ());
        return strings.toArray (emptystringarray);
    }
}
