#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <unistd.h>

enum
{
	max_threads = 32,
	limtime = 200,
	libre = 0,
	activo = 1,
	borrado = 2,
	suspendido = 3,
	dormido = 4
};

struct Thread
{
	ucontext_t ucp;
	int tid;
	long startq;
	void *stack;
	int status;
	long awaking;
};
typedef struct Thread Thread;

Thread threads[max_threads];

static int tid_count = 0, current_pos = 0;

static void handle_error(char *str)
{
	fprintf(stderr, "ERROR: %s\n", str);
	exit(EXIT_FAILURE);
}

static long gettimems()
{
	struct timeval now;

	gettimeofday(&now, NULL);

	return now.tv_sec * 1000 + now.tv_usec / 1000;
}

static int istime()
{
	long difference, now;

	now = gettimems();

	difference = now - threads[current_pos].startq;

	if(difference >= limtime)
		return 1;
	else
		return 0;
}

static int istimetoawake(int pos)
{
	long now;

	now = gettimems();

	if(now >= threads[pos].awaking)
		return 1;
	else
		return 0;
}

static int nexttoawake()
{
	int i, pos;

	for(i = 0; i < max_threads; i++)
		if(threads[i].status == dormido) {
			pos = i;
			break;
		}

	for(i = pos + 1; i < max_threads; i++)
		if(threads[i].status == dormido)
			if(threads[i].awaking < threads[pos].awaking)
				pos = i;

	return pos;
}

static int scheduler()
{
	int i, pos, sleep_flag = 0, suspend_flag = 0;

	for(i = 0; i < max_threads; i++) {
		pos = (current_pos + i + 1)%max_threads;

		if(threads[pos].status == borrado) {
			free(threads[pos].stack);
			threads[pos].status = libre;
		}

		if(threads[pos].status == suspendido)
			suspend_flag = 1;

		if(threads[pos].status == dormido) {
			if(istimetoawake(pos))
				threads[pos].status = activo;
			else
				sleep_flag = 1;
		}

		if(threads[pos].status == activo)
			return pos;
	}

	if(sleep_flag) {
		pos = nexttoawake();

		usleep(1000 * (threads[pos].awaking - gettimems()));

		threads[pos].status = activo;

		return pos;
	}

	if(suspend_flag)
		handle_error("SOLO QUEDAN THREADS SUSPENDIDOS");

	exit(0);
}

static int searchgap()
{
	int i, pos;

	for(i = 0; i < max_threads; i++) {
		pos = (current_pos + i + 1)%max_threads;

		if(threads[pos].status == borrado) {
			free(threads[pos].stack);
			threads[pos].status = libre;
		}

		if(threads[pos].status == libre)
			return pos;
	}

	return -1;
}

static void changethread(int pos)
{
	ucontext_t *oucp;
	
	oucp = &threads[current_pos].ucp;

	current_pos = pos;

	threads[pos].startq = gettimems();
	swapcontext(oucp, &threads[pos].ucp);
}

static void setthread(int pos)
{
	current_pos = pos;

	threads[pos].startq = gettimems();
	setcontext(&threads[pos].ucp);
}

static int search(int tid)
{
	int i;

	for(i = 0; i < max_threads; i++) {
		if(threads[i].tid == tid)
			return i;
	}

	return -1;
}

void initthreads(void)
{
	Thread thread;

	thread.tid = tid_count++;
	thread.startq = gettimems();
	thread.stack = NULL;
	thread.status = activo;
	thread.awaking = 0;

	threads[0] = thread;
}

int createthread(void (*mainf)(void*), void *arg, int stacksize)
{
	int pos;

	if((pos = searchgap()) < 0)
		return -1;

	void *p = malloc(stacksize);
	if(p == NULL)
		return -1;

	if(getcontext(&threads[pos].ucp) < 0)
		return -1;

	threads[pos].ucp.uc_link = NULL;
	threads[pos].ucp.uc_stack.ss_sp = p;
	threads[pos].ucp.uc_stack.ss_size = stacksize;
	makecontext(&threads[pos].ucp, (void (*)(void))mainf, 1, arg);

	threads[pos].tid = tid_count++;
	threads[pos].startq = 0;
	threads[pos].stack = p;
	threads[pos].status = activo;
	threads[pos].awaking = 0;

	return threads[pos].tid;
}

void exitsthread(void)
{
	int pos;

	threads[current_pos].status = borrado;

	pos = scheduler();

	setthread(pos);
}

void yieldthread(void)
{
	int pos;

	if(istime()) {
		if((pos = scheduler()) == current_pos) {
			threads[current_pos].startq = gettimems();
			return;
		}

		changethread(pos);
	}
}

int curidthread(void)
{
	return threads[current_pos].tid;
}

void suspendthread(void)
{
	int pos;

	threads[current_pos].status = suspendido;

	pos = scheduler();
	
	changethread(pos);
}

int resumethread(int id)
{
	int pos;

	if((pos = search(id)) < 0)
		return -1;

	if(threads[pos].status != suspendido)
		return -1;

	threads[pos].status = activo;

	return 0;
}

int suspendedthreads(int **list)
{
	int i, j = 0, count = 0;

	for(i = 0; i < max_threads; i++) {
		if(threads[i].status == suspendido)
			count++;
	}

	*list = malloc(count * sizeof(int));

	for(i = 0; i < max_threads; i++) {
		if(threads[i].status == suspendido) {
			(*list)[j] = threads[i].tid;
			j++;
		}
	}

	return count;
}

int killthread(int id)
{
	int pos;

	if((pos = search(id)) < 0)
		return -1;

	if(threads[pos].status == libre)
		return -1;

	if(pos == current_pos)
		return -1;

	threads[pos].status = libre;
	free(threads[pos].stack);

	return threads[pos].tid;
}

void sleepthread(int msec)
{
	int pos;

	threads[current_pos].status = dormido;
	threads[current_pos].awaking = gettimems() + msec;

	pos = scheduler();

	changethread(pos);
}