all:
	gcc sender.c -Wall -o sender
	gcc receiver.c -Wall -o receiver

clean:
	rm output/* | \
	rm -r output | \
	rm sender receiver | \
	clear 
