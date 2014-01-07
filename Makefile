SUBDIRS = $(LIBMOWGLI) src modules
CLEANDIRS = $(SUBDIRS) include
DISTCLEAN = extra.mk \
	buildsys.mk \
	config.log \
	config.status

include extra.mk
include buildsys.mk
