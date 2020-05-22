################ Source files ##########################################

test/srcs	:= $(wildcard test/*.c)
test/tsrcs	:= $(wildcard test/?????.c)
test/asrcs	:= $(filter-out ${test/tsrcs}, ${test/srcs})
test/tests	:= $(addprefix $O,$(test/tsrcs:.c=))
test/tobjs	:= $(addprefix $O,$(test/tsrcs:.c=.o))
test/aobjs	:= $(addprefix $O,$(test/asrcs:.c=.o))
test/objs	:= ${test/tobjs} ${test/aobjs}
test/deps	:= ${test/tobjs:.o=.d} ${test/aobjs:.o=.d}
test/outs	:= ${test/tobjs:.o=.out}

################ Compilation ###########################################

.PHONY:	test/all test/run test/clean test/check

test/all:	${test/tests}

# The correct output of a test is stored in testXX.std
# When the test runs, its output is compared to .std
#
check:		test/check
test/check:	${test/tests}
	@for i in ${test/tests}; do \
	    test="test/$$(basename $$i)";\
	    echo "Running $$test";\
	    $$i < $$test.c &> $$i.out;\
	    diff $$test.std $$i.out && rm -f $$i.out;\
	done

${test/tests}: $Otest/%: $Otest/%.o ${test/aobjs} ${liba}
	@echo "Linking $@ ..."
	@${CC} ${ldflags} -o $@ $^

################ Maintenance ###########################################

clean:	test/clean
test/clean:
	@if [ -d ${builddir}/test ]; then\
	    rm -f ${test/tests} ${test/objs} ${test/deps} ${test/outs} $Otest/.d;\
	    rmdir ${builddir}/test;\
	fi

${test/objs}: Makefile test/Module.mk ${confs} $Otest/.d

-include ${test/deps}
