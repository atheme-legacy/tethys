# Additional extensions for building single-file modules.
# This file mostly stolen from Atheme

.SUFFIXES: $(PLUGIN_SUFFIX)

plugindir=$(prefix)/modules/$(MODULE)
PLUGIN=${SRCS:.c=$(PLUGIN_SUFFIX)}

all: $(PLUGIN)
install: $(PLUGIN)

ifndef V
COMPILE_MODULE_STATUS = printf "CompileModule: $@\n"
else
COMPILE_MODULE_STATUS = printf "CompileModule: $${COMPILE_COMMAND}\n"
endif

cmd_cc_module = ${CC} ${DEPFLAGS} ${CFLAGS} ${PLUGIN_CFLAGS} \
                ${CPPFLAGS} ${PLUGIN_LDFLAGS} ${LDFLAGS} \
                -o $@ $< ${LIBS}

.c$(PLUGIN_SUFFIX):
	COMPILE_COMMAND="$(cmd_cc_module)"; \
	${COMPILE_MODULE_STATUS}; \
	$${COMPILE_COMMAND}
