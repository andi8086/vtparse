all: libvtparse.a vt100 ntest

PROG_OBJs = \
        vt100.o

CFLAGS += \
        -I./vtparse \
        -O2 \
        -Wall

LDFLAGS += \
        -L. \
        -L./vtparse \
        -lvtparse \
        -lncursesw

clean:
	$(MAKE) -C vtparse clean
	rm -f vt100 ntest *.o

libvtparse.a:
	$(MAKE) -C vtparse all

vt100: libvtparse.a $(PROG_OBJs)
	gcc -o $@ $(PROG_OBJs) $(LDFLAGS)

ntest: ntest.c
	gcc $(CFLAGS) -o $@ $^ -L. -lncursesw

.c.o:
	gcc $(CFLAGS) -o $@ -c $<

.PHONY: all clean

