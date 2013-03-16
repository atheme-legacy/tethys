#!/bin/awk -f

BEGIN {
  FS="\t";
  print "/* auto-generated from numeric.tab */";
  print "#ifndef __INC_NUMERIC_H__";
  print "#define __INC_NUMERIC_H__";
}

{
  if ($1 != "") {
    printf("#define %s %s\n", $1, $2)
    printf("#define NUMERIC_STR_%s %s\n", $2, $3)
  }
}

END {
  print "#define __u_numeric_fmt(x) NUMERIC_STR_##x";
  print "#define u_numeric_fmt(x) __u_numeric_fmt(x)";
  print "#endif";
}
