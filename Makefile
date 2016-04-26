scsh: scsh.o input.o bg_task.o history.o
	gcc scsh.o input.o bg_task.o history.o -o scsh

all:
	make clean
	make scsh

scsh.o: scsh.c
	gcc -c scsh.c

input.o: input.c
	gcc -c input.c

bg_task.o: bg_task.c
	gcc -c bg_task.c

history.o: history.c
	gcc -c history.c

clean:
	rm -f *.o scsh
