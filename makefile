shell: shell.o
	gcc -std=c99 -o shell shell.o

shell.o: shell.c
	gcc -std=c99 -c shell.c

clean:
	rm *.o shell