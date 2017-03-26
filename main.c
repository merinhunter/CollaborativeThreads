#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "threads.h"

enum
{
	stacksize = 32*1024,
	nthreads = 5
};

void func()
{
	int tid = curidthread();

	printf("Hola, soy %d\n", tid);

	if(tid == (nthreads - 1)) {
		printf("Matamos al thread %d\n", (tid - 1));
		if(killthread(tid - 1) != (tid - 1))
			fprintf(stderr, "NO SE PUEDE MATAR AL THREAD CON TID %d\n", (tid - 1));
	}

	printf("Suspendemos %d\n", tid);

	suspendthread();

	printf("%d se va\n", tid);

	exitsthread();
}

void resumethreads(int *list, int count)
{
	int i;

	if(count == 0)
		printf("No hay ningún thread suspendido.\n");
	else {
		printf("Hay %d threads suspendidos:\n", count);

		for(i = 0; i < count; i++) {
			printf("%d\n", list[i]);
			if(resumethread(list[i]) < 0)
				fprintf(stderr, "NO SE PUEDE REANUDAR EL THREAD CON TID %d\n", list[i]);
		}
	}

	free(list);
}

int main(int argc, char *argv[])
{
	int i, *list, count;

	initthreads();

	for(i = 1; i < nthreads; i++) {
		if(createthread((void *)func, NULL, stacksize) < 0)
			fprintf(stderr, "NO SE PUEDE CREAR EL THREAD %d\n", i);
	}

	printf("Main se va a dormir\n");
	sleepthread(10000);

	count = suspendedthreads(&list);

	resumethreads(list, count);

	printf("Main se va\n");

	exitsthread();
}