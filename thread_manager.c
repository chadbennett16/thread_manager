#define _GNU_SOURCE

void __attribute__ ((destructor)) Cleanup(void);

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mythreads.h"

typedef struct node {		//Node containing a currently running thread
	ucontext_t * context;
	int thread_id;
	int hasExited;
	void * result;
	struct node * next;
} node_t;

typedef struct thread_list { //List containing all running threads
	node_t * list_head;
	int num_threads;
} thread_list_t;


static int id_count = 1;
node_t * current = NULL;
thread_list_t * thread_list = NULL;

int interruptsAreDisabled = 0;

static int locks[NUM_LOCKS];
static int cond_count[NUM_LOCKS][CONDITIONS_PER_LOCK];

static void interruptDisable() {
	assert(!interruptsAreDisabled);
	interruptsAreDisabled = 1;
}

static void interruptEnable() {
	assert(interruptsAreDisabled);
	interruptsAreDisabled = 0;
}

void Cleanup (void) {
	interruptDisable();
	while (thread_list->list_head->next != thread_list->list_head) {
		node_t * iter = thread_list->list_head->next;
		thread_list->list_head->next = iter->next;
		iter->next = NULL;
		free(iter->context->uc_stack.ss_sp);
		free(iter->context);
		free(iter);
	}

	free(thread_list->list_head->context->uc_stack.ss_sp);
	free(thread_list->list_head->context);
	free(thread_list->list_head);
	free(thread_list);
	interruptEnable();
	exit(1);
}

void * FuncDone(thFuncPtr funcPtr, void *argPtr) { //Wrapper function

	void * result;
	result = funcPtr(argPtr);
	threadExit(result);

	return result;
}

/*void printThreads(void) {
	node_t * iter = thread_list->list_head;
	do{
		printf("Thread #%d\n", iter->thread_id);
		iter = iter->next;
	} while (iter != thread_list->list_head);
}*/

int FindID(int id) { //Finds if the id in the thread list exists, returns id if found, -1 if it doesn't exist
	interruptDisable();
	node_t * iter = thread_list->list_head;
	int i = 0;
	for (i = 0; i < thread_list->num_threads; i++) {

		if (iter->thread_id == id) {
			interruptEnable();			
			return id;
		}
		else iter = iter->next;		

	}
	interruptEnable();
	return -1;

}

int FindIDexit(int id) { //Checks if an ID has exited
	interruptDisable();
	node_t * iter = thread_list->list_head;
	int i;

	for (i = 0; i < thread_list->num_threads; i++) {

		if (iter->thread_id == id && iter->hasExited == 1) {
			interruptEnable();
			return 1;
		}		
		else iter = iter->next;
	}
	interruptEnable();
	return 0;

}

void * FindResult(int id) {
	interruptDisable();
	node_t * iter = thread_list->list_head;
	int i;
	for (i = 0; i < thread_list->num_threads; i++) {
		if (iter->thread_id == id) {
			interruptEnable();
			return iter->result;
		}
		else iter = iter->next;
	}
	interruptEnable();
	return NULL;
}

void threadInit() {
	interruptDisable();
	int i, j;
	for (i = 0; i < NUM_LOCKS; i++) locks[i] = -1;
	for (i = 0; i < NUM_LOCKS; i++) {
		for (j = 0; j < CONDITIONS_PER_LOCK; j++) {		
		 cond_count[i][j] = 0;
		}
	}
	//Initialize a list of threads
	thread_list = malloc(sizeof(thread_list_t));
	thread_list->list_head = malloc(sizeof(node_t));

	thread_list->num_threads = 1;
	thread_list->list_head->thread_id = 0;
	thread_list->list_head->hasExited = 0;
	thread_list->list_head->next = thread_list->list_head;
	thread_list->list_head->context = NULL;
	
	current = thread_list->list_head; 

	ucontext_t * main_context = malloc(sizeof(ucontext_t));
	getcontext(main_context);

	thread_list->list_head->context = main_context;	
	interruptEnable();
}

int threadCreate(thFuncPtr funcPtr, void *argPtr) {
	interruptDisable();
	//Create a new node
	int id = id_count;
	id_count++;
	node_t * new_thread = malloc(sizeof(node_t));
	new_thread->thread_id = id;
	new_thread->hasExited = 0;
	new_thread->next = current->next;
	current->next = new_thread;

	thread_list->num_threads++;	

	ucontext_t * newcontext = malloc(sizeof(ucontext_t));

	getcontext(newcontext);

	newcontext->uc_stack.ss_sp = malloc(STACK_SIZE);
	newcontext->uc_stack.ss_size = STACK_SIZE;
	newcontext->uc_stack.ss_flags = 0;

	new_thread->context = newcontext;		//Save the new context in the list

	ucontext_t *prev_context = current->context;
	current = current->next;

	interruptEnable();
	makecontext(current->context, (void(*)(void))FuncDone, 2, (void(*)(void))funcPtr, argPtr);
	swapcontext(prev_context, current->context);

	return id;

}

void threadYield() {
	interruptDisable();
	node_t * iter = current->next;

	while (iter->hasExited == 1) {

		iter = iter->next;
	}

	ucontext_t * yield_context = current->context;
	current = iter;
	interruptEnable();
	swapcontext(yield_context, iter->context);
	return;

} 

void threadJoin(int thread_id, void **result) {

	if (FindID(thread_id) == -1) {
		return;
	}
	else {
		while (!FindIDexit(thread_id)) {
			threadYield();
		}

		*result = FindResult(thread_id);
		return;
	}

}

//exits the current thread -- closing the main thread, will terminate the program
void threadExit(void *result) {	
	if (current->thread_id == 0) {
		Cleanup();		
	}
	else {

		current->hasExited = 1;
		current->result = result;

		threadYield();
	}

} 

void threadLock(int lockNum) {
//wait for the specified lock number to be available and get it
	if (lockNum > NUM_LOCKS - 1 || lockNum < 0) return;
	while (locks[lockNum] != -1) { 	//Lock is not available, so wait
		threadYield();
	}
	interruptDisable();
	locks[lockNum] = current->thread_id;
	interruptEnable();
}
 
void threadUnlock(int lockNum) {
//unlock the specified lock number
	if (lockNum > NUM_LOCKS - 1 || lockNum < 0) return;
	else if (locks[lockNum] == -1) return;
	else if (locks[lockNum] == current->thread_id) {
		interruptDisable();
		locks[lockNum] = -1;
		interruptEnable();
	}
}

void threadWait(int lockNum, int conditionNum) {
	if (locks[lockNum] == -1) {
		printf("Error! Mutex is not locked\n");
		exit(0);
	}
	else{
		threadUnlock(lockNum);
		while (cond_count[lockNum][conditionNum] < 1) {
			threadYield();
		}
		threadLock(lockNum);
		interruptDisable();
		cond_count[lockNum][conditionNum] = 0;
		interruptEnable();
	}
}

void threadSignal(int lockNum, int conditionNum) {
	interruptDisable();
	cond_count[lockNum][conditionNum]++;
	interruptEnable();
} 
