
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

// -----------------------------------
static int qid; // update: add current queue index/priority
// -----------------------------------

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;

		// update: add numslots init
		mlq_ready_queue[i].numslots = MAX_PRIO - i;
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);

	// update: init current queue index
	qid = 0;
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);

	int empty_queue_count = 0;
	while (mlq_ready_queue[qid].numslots <= 0 || empty(&mlq_ready_queue[qid]))
	{ // if current queue is out of slot or it is empty
		
		if (mlq_ready_queue[qid].numslots <= 0 && !empty(&mlq_ready_queue[qid]))
			mlq_ready_queue[qid].numslots = MAX_PRIO - qid; // reset current queue's numslot (for future slots)

		++empty_queue_count;

		if (empty_queue_count == MAX_PRIO) {
			#ifdef SCHED_DEBUG
			printf("All queues are empty, wait...\n");
			#endif
			pthread_mutex_unlock(&queue_lock);
			return NULL; // all queues are empty
		}

		// go to next queue
		if (qid == MAX_PRIO - 1)
			qid = 0;
		else
			qid++;
	}
	
	// numslot management (maybe)
	// ...
	
	proc = dequeue(&mlq_ready_queue[qid]);
	pthread_mutex_unlock(&queue_lock);
	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	proc->numslots = &mlq_ready_queue[proc->prio].numslots; // update: add numslots pointer init
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


