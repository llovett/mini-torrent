OBJECTS=bencode-tools-2011-03-15/bencode.o
INCLUDES=bencode-tools-2011-03-15/include/ 
EXE=hw4

all:
	echo "Don't forget to install OpenSSL and libcurl3-dev"
	gcc -g --std=gnu99 -I $(INCLUDES) hw4.c hw4_bencode.c $(OBJECTS) -o $(EXE) -lssl -lcurl

clean:
	@rm -f $(EXE)
	@rm -f *.o *~
