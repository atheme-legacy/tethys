#ifndef __INC_CONF_H__
#define __INC_CONF_H__

#define U_CONF_MAX_KEY 128
#define U_CONF_MAX_VALUE 512

extern void u_conf_read();
/* FILE *f, void (*cb)(char *key, char *value)

   cb is called with key=>value pairs. The key is a dot-separated string
   indicating the path to the value.

   The following config file:
      me {
        name micro.irc
        sid 22U
        admin {
          name "Alex Iadicicco (aji)"
          email "alex@ajitek.net"
        }
      }
      link {
        name hub.us
        ip 1.2.3.4
      }
   will have the following cb sequence:
      cb("me.name", "micro.irc");
      cb("me.sid", "22U");
      cb("me.admin.name", "Alex Iadicicco (aji)");
      cb("me.admin.email", "alex@ajitek.net");
      cb("link.name", "hub.us");
      cb("link.ip", "1.2.3.4");

   A block is started by a { where a value is expected, and is closed with a }
   where a key is expected. Values whose first character is " will be read
   byte-for-byte until an unescaped " is read. Keys and all other values
   are read until a char for which isspace() returns true is read. Note that
   this means the above config could be formatted as follows:
      me.name micro.irc
      me.sid 22U
      me.admin {
        name "Alex Iadicicco (aji)"
        email "alex@ajitek.net"
      }
      link.name hub.us
      link.ip 1.2.3.4

   Comments start with a # and continue to end of line. Comments inside
   quoted values will not be skipped.

   */

#endif
