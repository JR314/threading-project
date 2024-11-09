.phony all:
all: mts

mts: mts.c
	gcc -pthread -o mts mts.c


.PHONY clean:
clean:
	-rm -rf *.o *.exe
