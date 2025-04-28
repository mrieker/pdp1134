export CLASSPATH=`dirname $0`
java KiCadToJLCBOM < zturn11-bom.csv > zturn11-jlc-bom.csv
java KiCadToJLCCPL < zturn11-all-pos.csv > zturn11-jlc-pos.csv
