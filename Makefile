GCC=gcc -g -Wall 

all : quicksort

quicksort: quicksort.o sched.o sched.h
	$(GCC) -o quicksort quicksort.o sched.o -pthread

quicksort.o: quicksort.c sched.h
	$(GCC) -o quicksort.o -c quicksort.c

sched.o: sched.c sched.h
	$(GCC) -o sched.o -c sched.c

clean : 
	rm *.o quicksort
