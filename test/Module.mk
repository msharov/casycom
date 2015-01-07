################ Source files ##########################################

test/SRCS	:= $(wildcard test/*.c)
test/TESTS	:= $(addprefix $O,$(test/SRCS:.c=))
test/OBJS	:= $(addprefix $O,$(test/SRCS:.c=.o))
test/DEPS	:= ${test/OBJS:.o=.d}

################ Compilation ###########################################

.PHONY:	test/all test/run test/clean test/check

test/all:	${test/TESTS}

# The correct output of a test is stored in testXX.std
# When the test runs, its output is compared to .std
#
check:		test/check
test/check:	${test/TESTS}
	@for i in ${test/TESTS}; do \
	    TEST="test/$$(basename $$i)";\
	    echo "Running $$TEST";\
	    $$i < $$TEST.c &> $$i.out;\
	    diff $$TEST.std $$i.out && rm -f $$i.out;\
	done

${test/TESTS}: $Otest/%: $Otest/%.o ${LIBA}
	@echo "Linking $@ ..."
	@${CC} ${LDFLAGS} -o $@ $^

################ Maintenance ###########################################

clean:	test/clean
test/clean:
	@if [ -d $O/test ]; then\
	    rm -f ${test/TESTS} ${test/OBJS} ${test/DEPS};\
	    rmdir $O/test;\
	fi

${test/OBJS}: Makefile test/Module.mk ${CONFS} $O.d ${NAME}/config.h

-include ${test/DEPS}
