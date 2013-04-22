CC=g++
CFLAGS=-Wall

all: main.o
	g++ -o cecanyway main.o -ldl

main.o: main.cpp
	$(CC) -c main.cpp

clean:
	rm main.o cecanyway

install: all
	cp cecanyway /usr/bin/
	cp cecanyway.init /etc/init.d/cecanyway
	chmod ug+x /etc/init.d/cecanyway

uninstall:
	rm /usr/bin/cecanyway
	rm /etc/init.d/cecanyway
