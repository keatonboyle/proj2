HEADERS = util.h rdtimpl.h

all: recv send files

%.o: %.c $(HEADERS)
	gcc -c -o $@ $<

send: sender.o util.o rdtimpl.o
	gcc -o ./send/sender sender.o util.o rdtimpl.o
	
recv: receiver.o util.o rdtimpl.o
	gcc -o ./recv/receiver receiver.o util.o rdtimpl.o

files:
	rm -f ./recv/*.t

clean: files
	rm -f *.o
	rm -f ./recv/receiver
	rm -f ./send/sender
