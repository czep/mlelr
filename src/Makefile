CC=gcc
CFLAGS=-Wall -g -pg -lm -lgsl -lgslcblas

mlelr: main.o csv.o dataset.o model.o interface.o tabulate.o mlelr.o 
	$(CC) -o mlelr main.o csv.o dataset.o model.o interface.o tabulate.o mlelr.o $(CFLAGS)

clean:
	rm -f mlelr gmon.out main.o csv.o dataset.o model.o interface.o tabulate.o mlelr.o 
