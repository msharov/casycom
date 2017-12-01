-include Config.mk

################ Source files ##########################################

SRCS	:= $(wildcard *.c)
INCS	:= $(addprefix ${NAME}/,$(filter-out ${NAME}.h,$(sort $(wildcard *.h) config.h)))
OBJS	:= $(addprefix $O,$(SRCS:.c=.o))
DEPS	:= ${OBJS:.o=.d}
CONFS	:= Config.mk config.h casycom.pc
ONAME   := $(notdir $(abspath $O))
DOCS	:= $(notdir $(wildcard doc/*))
LIBA_R	:= $Olib${NAME}.a
LIBA_D	:= $Olib${NAME}_d.a
ifdef DEBUG
LIBA	:= ${LIBA_D}
else
LIBA	:= ${LIBA_R}
endif

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
	@${CC} ${CFLAGS} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${CFLAGS} -S -o $@ -c $<

################ Installation ##########################################

.PHONY:	install uninstall

ifdef INCDIR
INCSI		:= $(addprefix ${INCDIR}/,${INCS})
INCR		:= ${INCDIR}/${NAME}.h
install:	${INCSI} ${INCR}
${INCSI}: ${INCDIR}/${NAME}/%.h: %.h
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@
${INCR}:	${NAME}.h
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@
uninstall:	uninstall-incs
uninstall-incs:
	@if [ -d ${INCDIR}/${NAME} ]; then\
	    echo "Removing headers ...";\
	    rm -f ${INCSI} ${INCR};\
	    ${RMPATH} ${INCDIR}/${NAME};\
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
ifdef DOCDIR
PKGDOCDIR	:= ${DOCDIR}/${NAME}
DOCSI		:= $(addprefix ${PKGDOCDIR}/,${DOCS})
install:	${DOCSI}
${DOCSI}: ${PKGDOCDIR}/%: doc/%
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@
uninstall:	uninstall-docs
uninstall-docs:
	@if [ -d ${PKGDOCDIR} ]; then\
	    echo "Removing documentation ...";\
	    rm -f ${DOCSI};\
	    ${RMPATH} ${PKGDOCDIR};\
	fi
endif
ifdef PKGCONFIGDIR
PCI	:= ${PKGCONFIGDIR}/casycom.pc
install:	${PCI}
${PCI}:	casycom.pc
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@

uninstall:	uninstall-pc
uninstall-pc:
	@if [ -f ${PCI} ]; then echo "Removing ${PCI} ..."; rm -f ${PCI}; fi
endif

################ Maintenance ###########################################

include test/Module.mk

clean:
	@if [ -h ${ONAME} ]; then\
	    rm -f ${LIBA_R} ${LIBA_D} ${OBJS} ${DEPS} $O.d ${ONAME};\
	    ${RMPATH} ${BUILDDIR};\
	fi

distclean:	clean
	@rm -f ${CONFS} config.status

maintainer-clean: distclean

$O.d:	${BUILDDIR}/.d
	@[ -h ${ONAME} ] || ln -sf ${BUILDDIR} ${ONAME}
$O%/.d:	$O.d
	@[ -d $(dir $@) ] || mkdir $(dir $@)
	@touch $@
${BUILDDIR}/.d:	Makefile
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@touch $@

Config.mk:	Config.mk.in
config.h:	config.h.in
casycom.pc:	casycom.pc.in
${OBJS}:	Makefile ${CONFS} $O.d config.h
${CONFS}:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ...";\
	    ./config.status;\
	else echo "Running configure ...";\
	    ./configure;\
	fi

-include ${DEPS} ${DEPS}
