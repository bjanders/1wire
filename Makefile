LDFLAGS = -lusb
CFLAGS = -Wall -g


all: test3 test2 owmodule

test2: test2.c ds2490.o util.o
test3: test3.c ds2490.o util.o

owmodule: owmodule.c ds2490.o
	python setup.py build

clean:
	-rm *.o test2 test3
