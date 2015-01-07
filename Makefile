-include Config.mk

################ Source files ##########################################

LIBA	:= $Olib${NAME}.a
SRCS	:= $(wildcard *.c)
INCS	:= ${NAME}.h $(addprefix ${NAME}/,$(filter-out ${NAME}.h,$(sort $(wildcard *.h) config.h)))
OBJS	:= $(addprefix $O,$(SRCS:.c=.o))
DEPS	:= ${OBJS:.o=.d}
CONFS	:= Config.mk config.h
ONAME   := $(notdir $(abspath $O))

################ Compilation ###########################################

.PHONY: all clean distclean maintainer-clean

all:	${LIBA}

${LIBA}:	${OBJS}
	@echo "Linking $@ ..."
	@rm -f $@
	@${AR} qc $@ $^
	@${RANLIB} $@

$O%.o:	%.c
	@echo "    Compiling $< ..."
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${CFLAGS} -S -o $@ -c $<

################ Installation ##########################################

.PHONY:	install uninstall

ifdef INCDIR
INCSI		:= $(addprefix ${INCDIR}/,${INCS})
install:	${INCSI}
${INCSI}: ${INCDIR}/%.h: %.h
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@
uninstall:	uninstall-incs
uninstall-incs:
	@if [ -d ${INCDIR}/${NAME} ]; then\
	    echo "Removing headers ...";\
	    rm -f ${INCSI};\
	    rmdir ${INCDIR}/${NAME};\
	fi
endif
ifdef LIBDIR
LIBAI		:= ${LIBDIR}/$(notdir ${LIBA})
install:        ${LIBAI}
${LIBAI}:       ${LIBA}
	@echo "Installing $@ ..."
	@${INSTALLLIB} $< $@
uninstall:	uninstall-lib
uninstall-lib:
	@if [ -f ${LIBAI} ]; then\
	    echo "Removing ${LIBAI} ...";\
	    rm -f ${LIBAI};\
	fi
endif

################ Maintenance ###########################################

include test/Module.mk

clean:
	@if [ -h ${ONAME} ]; then\
	    rm -f $O.d ${LIBA} ${OBJS} ${DEPS} ${ONAME};\
	    rmdir ${BUILDDIR};\
	fi

distclean:	clean
	@rm -f ${CONFS} ${NAME} config.status

maintainer-clean: distclean

$O.d:   ${BUILDDIR}/.d
	@[ -h ${ONAME} ] || ln -sf ${BUILDDIR} ${ONAME}
${BUILDDIR}/.d:     Makefile
	@mkdir -p ${BUILDDIR} && touch ${BUILDDIR}/.d

Config.mk:	Config.mk.in
config.h:	config.h.in
${OBJS}:	Makefile ${CONFS} $O.d ${NAME}/config.h
${NAME}/config.h:	config.h
	@rm -f ${NAME}; ln -s . ${NAME}
${CONFS}:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ...";\
	    ./config.status;\
	else echo "Running configure ...";\
	    ./configure;\
	fi

-include ${DEPS} ${DEPS}
