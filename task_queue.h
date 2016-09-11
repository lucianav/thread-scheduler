/* Luciana Viziru - 332CA */
#include <pthread.h>
#include "so_scheduler.h"

typedef enum {NEW, READY, RUNNING, WAITING, TERMINATED} th_state;

typedef struct task_queue task_queue;

typedef struct task {
	tid_t thread;					/* unique thread id */
	th_state state;					/* thread state */
	pthread_mutex_t thread_mutex;	/* mutex used for thread block/unblock */
	so_handler *handler;			/* handler to be executed */
	unsigned time_done;				/* number of intstructions executed */
	unsigned priority;				/* thread priority */
	unsigned queue_id;				/* id of thread used in task queue logic */
} task;

/* priority queue constructor */
task_queue *init_task_queue(void);

/* add task to task_queue */
void add(task_queue *q, task *t);

/* get element at the root of the task_queue - NULL for empty task_queue */
task *peek(task_queue *q);

/* get and remove element at the root of the task_queue */
/* NULL for empty task_queue */
task *pop(task_queue *q);

/* task_queue destructor */
void destruct_task_queue(task_queue *q);
