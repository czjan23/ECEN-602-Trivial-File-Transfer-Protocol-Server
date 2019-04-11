all: tftps

tftps: tftps.c message.c
	gcc -o $@ $^

clean:
	-rm tftp tftps
	-rm *.o