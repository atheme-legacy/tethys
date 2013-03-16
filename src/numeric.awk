#!/bin/awk -f

# ircd-micro, numeric.awk -- numeric.[hc] generator
# Copyright (C) 2013 Alex Iadicicco
#
# This file is protected under the terms contained
# in the COPYING file in the project root


BEGIN {
  FS="\t";
  HDR="numeric.h";
  SRC="numeric.c";

  print "/* auto-generated from numeric.tab */" > HDR;
  print "#ifndef __INC_NUMERIC_H__" > HDR;
  print "#define __INC_NUMERIC_H__" > HDR;
  print "extern char *u_numeric_fmt[];" > HDR;

  print "/* auto-generated from numeric.tab */" > SRC;
  print "#define NULL ((char*)0)" > SRC;
  print "char *u_numeric_fmt[] = {" > SRC;
  nextnum = 0;
}

{
  if ($1 != "") {
    printf("#define %s %s\n", $1, $2) > HDR;
    if ($3 == "")
      $3 = "NULL";
    while (nextnum <= $2) {
      if (nextnum != $2)
        print "  NULL," > SRC;
      nextnum++;
    }
    printf("  %s, /* %s %s */\n", $3, $1, $2) > SRC;
  }
}

END {
  print "#endif" > HDR;

  print "};" > SRC;
}
