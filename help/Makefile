HELPFILES = $(wildcard opers/*) $(wildcard users/*)

include ../buildsys.mk
include ../extra.mk

helpdir = ${prefix}/help

install-extra:
	for i in ${HELPFILES}; do \
		${INSTALL_STATUS}; \
		if ${MKDIR_P} $$(dirname ${DESTDIR}${helpdir}/$$i) && ${INSTALL} -m 644 $$i ${DESTDIR}${helpdir}/$$i; then \
			${INSTALL_OK}; \
		else \
			${INSTALL_FAILED}; \
		fi; \
	done;
