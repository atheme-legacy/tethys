# ircd-micro 0.1

## About

**ircd-micro** is an attempt to write a basic TS6 IRC daemon from
scratch. This was done more as a personal challenge rather than as a
real practical project.

The goal is to serve as a solid base for researching IRC software,
providing a lightweight framework within which to build a more robust
IRCD.


## Contact

The official ircd-micro channel is #micro on irc.interlinked.me. I will
be using one of many nicknames, but will definitely have +o. Users with
+o are contributors.


## Compiling

The Makefile is in `src`. A helper script called `build` has been
written to assist in interacting with the Makefile in the project root
directory. It's very simple. Anybody familiar with shell syntax should
have no problem deciphering it.

The executable `ircd-micro` will be created in `src`. To install, place it
somewhere in your `$PATH`. It is mostly standalone, however it needs to
know where to find the relevant config files. It first looks for them in
the directory called `etc`, relative to the current working directory. If
the files are somewhere else, you may specify their location with the
appropriate options. Invoke ircd-micro with `-h` to get a list of options.


## Running

Configuration goes in `etc/micro.conf`. An example configuration file can
be found in `doc/micro.conf.example`


## Contributing

Since ircd-micro is still in the very early stages of development, a lot
of the active coding work is related to the organizational structure of
the codebase, and I only plan to accept patches which I feel fit into
my plan for that.

That being said, there is still a lot of coding work that can be done. I'm
currently using GitHub's "Issues" feature to keep track of tasks that
need to be completed before the 1.0 release of ircd-micro. I've marked
tasks that I would be willing to accept outside help for with the label
"open". I will consider patches for other components as well, but I
would really like those to be discussed with me ahead of time, since I
probably already have something planned in that area and would like to
be involved in the design of that component.


## Legacy

This project used to be an attempt to build an IRCD for the 4.3 BSD
operating system. However, the project matured and its direction changed,
and this aspect of the design was beginning to become an obstruction. In
a 4.3 BSD system, here is no SSL, no dynamic loading, no robust `crypt()`
implementation, no `vsnprintf()`, no C89 compiler, no `make` with Makefile
inclusion, and many other limitations and idiosyncracies. It started to get in the way of meaningful progress.

This project still exists, in spirit, at
[aji/bsdchat](http://github.com/aji/bsdchat).
I will, from time to time, backport features patches from
ircd-micro to bsdchat, so general IRCD features should be targeted to
ircd-micro. Obviously, BSD-specific patches should be submitted to the
bsdchat project.
