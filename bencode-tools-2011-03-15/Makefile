AR = ar
CC = gcc
CFLAGS = -Wall -O2 -g -fPIC -shared -Iinclude 
EXECFLAGS = -Wall -O2 -g -Iinclude 
LDFLAGS = 
PREFIX = /usr/local

LIBMODULES = bencode.o

all:	libbencodetools.so bencodetest bencat

libbencodetools.so:	$(LIBMODULES)
	$(CC) -shared -Wl,-soname,libbencodetools.so -o libbencodetools.so $(LIBMODULES)

bencode.o:	bencode.c include/bencodetools/bencode.h

bencodetest:	bencodetest.c libbencodetools.so
	$(CC) $(EXECFLAGS) -o $@ $< -L. -Wl,-rpath,. -lbencodetools

bencat:	bencat.c libbencodetools.so
	$(CC) $(EXECFLAGS) -o $@ -L. $(LDFLAGS) $< -lbencodetools

check:	bencodetest
	./bencodetest
	@echo
	@echo "All tests successful"

install:	
	mkdir -p -m 755 "$(PREFIX)/lib" "$(PREFIX)/include/bencodetools" "$(PREFIX)/bin"
	install -m 644 include/bencodetools/*.h "$(PREFIX)/include/bencodetools/"
	install -m 644 libbencodetools.so "$(PREFIX)/lib/"
	install bencat "$(PREFIX)/bin/"

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

clean:	
	rm -f *.o *.so bencodetest bencat

