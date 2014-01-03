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


## Building

ircd-micro can be compiled and installed using the typical sequence:

    $ ./configure
    $ make
    $ make install

By default, ircd-micro will be installed into `~/ircd`, but this can be
changed with the `--prefix` argument to `./configure`.


## Running

Invoke ircd-micro with `-h` to get a list of options. By default,
configuration goes in `etc/micro.conf` relative to the current
directiory. An example configuration file can be found in
`doc/micro.conf.example`


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
a 4.3 BSD system, there is no SSL, no dynamic loading, no robust `crypt()`
implementation, no `vsnprintf()`, no C89 compiler, no `make` with Makefile
inclusion, and many other limitations and idiosyncracies. It started to
get in the way of meaningful progress.

This project still exists, in spirit, at
[aji/bsdchat](http://github.com/aji/bsdchat).
I will, from time to time, backport features patches from
ircd-micro to bsdchat, so general IRCD features should be targeted to
ircd-micro. Obviously, BSD-specific patches should be submitted to the
bsdchat project.
