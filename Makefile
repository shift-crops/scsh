scsh: scsh.o signal.o input.o parse.o bg_task.o history.o
	gcc scsh.o signal.o input.o parse.o bg_task.o history.o -o scsh

all:
	make clean
	make scsh

scsh.o: scsh.c
	gcc -c scsh.c

signal.o: signal.c
	gcc -c signal.c

input.o: input.c
	gcc -c input.c

parse.o: parse.c
	gcc -c parse.c

bg_task.o: bg_task.c
	gcc -c bg_task.c

history.o: history.c
	gcc -c history.c

clean:
	rm -f *.o scsh
