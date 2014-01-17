SUBDIRS = $(LIBMOWGLI) src modules
CLEANDIRS = $(SUBDIRS) include
DISTCLEAN = extra.mk \
	buildsys.mk \
	config.log \
	config.status

include buildsys.mk
include extra.mk
