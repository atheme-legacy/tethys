# Tethys 0.1

A next-generation replacement for charybdis.

## Why?

There are a few reasons why it is desirable to replace charybdis:

* It does not integrate well into the rest of the atheme platform due to
  being a ratbox fork, i.e. we decided to follow ratbox upstream and work
  with them on libratbox, meaning that we have our own libmowgli and there
  is also this libratbox thing and there's a lot of feature overlap...

* Bitrot from changes to changes to changes to 2.8.21 code, which is
  not well understood in terms of side effects, etc.

* Legal issues with the ratbox/hybrid-7 origin of charybdis, including:

  - Diane Bruce's decision to strip copyrights from headers, giving us
    very little idea of the origin of most 2.8.21 code ("ircd contributors
    past and present")

  - Multiple licenses used in the code (GPLv1, GPLv2, BSD-likes), some
    authors no longer being alive to relicense their code to BSD-like
    or allowing an exception to allow openssl to be linked against it

  - Binary redistributions of charybdis linked to openssl are illegal
    because of the above

* Many components of mowgli (VIO, for example) were designed with the
  intention of replacing charybdis


## Support

The official Tethys support channel is #tethys on irc.staticbox.net.


## Building

If this source has been obtained via git, the following commands should
be run first:

    $ git submodule init
    $ git submodule update

You will need to run `git submodule update` for each successive pull.
Tethys can be then compiled and installed using the typical sequence:

    $ ./configure
    $ make
    $ make install

By default, Tethys will be installed into `~/ircd`, but this can be
changed with the `--prefix` argument to `./configure`.


## Running

Invoke tethys with `-h` to get a list of options. By default,
configuration goes in `etc/tethys.conf` relative to the current
directiory. An example configuration file can be found in
`doc/tethys.conf.example`
