all: mterm_col mterm-test mterm test

CFLAGS+=-ggdb -W -Wextra $(shell gfxprim-config --cflags)
LDFLAGS+=$(shell gfxprim-config --libs --libs-backends) -lutil

mterm_col.o: CFLAGS+=-DMT_COLORS -DMT_RESIZE

MTERM_LIB=mt-screen.o mt-sbuf.o mt-parser.o

mterm-test: $(MTERM_LIB) mterm-test.o
mterm: $(MTERM_LIB) mterm.o
mterm_col: $(MTERM_LIB) mterm_col.o

test: mterm-test
	@echo "**************** Running tests ****************"
	@cd tests; ./run.sh

clean:
	rm -f mterm-test term term_col *.o
