


# ircd-micro 0.1

by Alex Iadicicco


## About

ircd-micro is an attempt to write an IRC daemon from scratch for the 4.3 BSD architecture. This was done more as a personal challenge rather than as a real practical project.

The goal is to be fully compatible with most modern TS6 IRC daemons, in particular charybdis ircd and atheme-services. This will allow a 4.3 BSD system running ircd-micro to be linked into existing TS6 networks.

## Contact

The official ircd-micro channel is #micro on irc.interlinked.me. I will be using one of many nicknames, but will definitely have +o. Users with +o are contributors.

## Compiling

The Makefile is in src. A helper script called 'build' has been written to assist in interacting with the Makefile in the project root directory. It's very simple. Anybody familiar with shell syntax should have no problem deciphering it.

The executable ircd-micro will be created in src. To install, place it somewhere in your $PATH. It is mostly standalone, however it needs to know where to find the relevant config files. It first looks for them in the directory called 'etc', relative to the current working directory. If the files are somewhere else, you may specify their location with the appropriate options. Invoke ircd-micro with -h to get a list of options.


## Running

Configuration goes in etc/micro.conf. An example configuration file can be found in doc/micro.conf.example


## Contributing

Since ircd-micro is still in the very early stages of development, a lot of the active coding work is related to the organizational structure of the codebase, and I only plan to accept patches which I feel fit into my plan for that.

That being said, there is still a lot of coding work that can be done. I'm  currently using GitHub's "Issues" feature to keep track of tasks that  need to be completed before the 1.0 release of ircd-micro. I've marked tasks that I would be willing to accept outside help for with the label "open". I will consider patches for other components as well, but I would really like those to be discussed with me ahead of time, since I probably already have something planned in that area and would like to be involved in the design of that component.

## Future

When I feel ircd-micro meets my original goals, I will bump it to version 1.0. At this point, I will drop 4.3 BSD support. Attempting to support 4.3 BSD is beginning to get in the way of productivity. As an example, variable length argument lists are done very differently in 4.3 BSD than in a modern GNU system. The codebase is full of nasty tricks to consolidate these differences. Additionally, the lack of parameter lists in function prototypes has led to some hard to find bugs, and now-commonplace functions like memmove() and inet_ntop() are missing entirely. It was fun before to work in conditions like this, but now it's just getting annoying.

I will still keep a "legacy" 4.3 BSD branch around, onto which new features and bugfixes will be backported as appropriate. However, trying to write code for two wildly different operating systems at the same time is beginning to become more work than it's worth, and I'd like to split them into separate projects if at all possible.
