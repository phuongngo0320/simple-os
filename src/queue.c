#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
	if (q->size == MAX_QUEUE_SIZE) // queue full
	{
		#ifdef SCHED_DEBUG
		printf("Queue is full\n");
		#endif
		abort();
	}
	if (proc == NULL)
	{
		#ifdef SCHED_DEBUG
		printf("Enqueue a NULL process!\n");
		#endif
	}

	q->proc[q->size] = proc;
	q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
	if (empty(q))
		return NULL;

	// update: remove process at the head (FIFO style)
	struct pcb_t* proc = q->proc[0];
	//int prio = q->proc[0]->prio;
	int pos = 0;

	// // find process with highest priority
	// for (int i = 1; i < q->size; ++i)
	// {
	// 	if (q->proc[i] == NULL)
	// 		return NULL; // error
		
	// 	if (q->proc[i]->prio < prio) 
	// 	{
	// 		proc = q->proc[i];
	// 		pos = i;
	// 	}
	// }

	// remove process from q
	for (int i = pos; i < q->size - 1; ++i)
		q->proc[i] = q->proc[i + 1];
	q->size--;
		
	return proc;
}

