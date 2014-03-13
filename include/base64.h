/* Tethys, base64.h -- base64 encoding and decoding
   Copyright (C) 2014 Aaron Jones

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_BASE64_H__
#define __INC_BASE64_H__

extern char* base64_encode(const unsigned char*, unsigned int);
extern unsigned char* base64_decode(const char*, unsigned int*);

#endif
