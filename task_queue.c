/* Luciana Viziru - 332CA */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "task_queue.h"
#include "util.h"

#define INIT_SIZE 8

typedef enum {HALF, DOUBLE} resize_type;

/* struct for task_queue */
struct task_queue {

	task **tasks;			/* task container */
	unsigned max_capacity;	/* maximum capacity of queue */
	unsigned size;			/* number of elements in queue */
	unsigned queue_id;		/* variable used in queue logic */
							/* counts queue add operations */
};

/* function used to comapre two tasks */
static int has_priority(task *t1, task *t2)
{
	/* if higher priority or equal priority and smaller queue_id */
	/* than t1 has priority */
	/* else t2 has priority */
	if (t1->priority > t2->priority ||
		(t1->priority == t2->priority && t1->queue_id < t2->queue_id)) {
		return 1;
	}
	else {
		return 0;
	}
}

/* check and mantain max-heap property for task_queue at pop */
static void heapify_down(task_queue *q)
{
	unsigned left, right, node, max_son;
	task *aux;

	node = 0;

	while (1) {
		/* handles for child nodes */
		left = node * 2 + 1;
		right = node * 2 + 2;

		/* if there are two sons, find the one with maximum priority */
		if (right < q->size) {
			max_son = has_priority(q->tasks[left],
									q->tasks[right]) ? left : right;
		}
		else {
			/* only one son, that has max_son priority */
			if (left < q->size) {
				max_son = left;
			}
			else {
				/* no child nodes, heapify is done */
				break;
			}
		}

		/* if necessary, swap nodes and repeat process */
		if (has_priority(q->tasks[max_son], q->tasks[node])) {
			aux = q->tasks[max_son];
			q->tasks[max_son] = q->tasks[node];
			q->tasks[node] = aux;
			node = max_son;
		}
		else {
			break;
		}
	}
}

/* check and mantain max-heap property for task_queue at insert */
static void heapify_up(task_queue *q)
{
	unsigned parent, node;
	task *aux;
	/* handle for last node in heap, just inserted */
	/* node >= 0, called just after insert */
	node = q->size - 1;

	/* node is now root, no swaps needed */
	if (node == 0) {
		return;
	}

	/* handles for parent node */
	parent = (node - 1) / 2;

	/* if necessary, swap nodes and repeat process */
	while (node > 0 && has_priority(q->tasks[node], q->tasks[parent])) {

		aux = q->tasks[parent];
		q->tasks[parent] = q->tasks[node];
		q->tasks[node] = aux;

		node = parent;
		parent = (node - 1) / 2;
	}
}

/* function that resizes container */
/* if there is no more free space */
static void resize(task_queue *q, resize_type t)
{
	DIE(q == NULL, "task_queue resize called with NULL parameter");
	unsigned new_capacity;

	if (t == DOUBLE) {
		new_capacity = q->max_capacity * 2;
	}
	else {
		if (t == HALF) {
			new_capacity = q->max_capacity / 2;
		}
	}

	q->tasks = realloc(q->tasks, new_capacity * sizeof(task **));
	DIE(q == NULL, "task_queue resize failed");
	q->max_capacity = new_capacity;
}


/* task_queue constructor */
task_queue *init_task_queue(void)
{
	int i;
	struct task_queue *q;

	/* initialize all task_queue data and alloc needed space */
	q = malloc(sizeof(struct task_queue));
	DIE(q == NULL, "malloc failed in task_queue");
	q->tasks = malloc(INIT_SIZE * sizeof(task *));
	DIE(q->tasks == NULL, "malloc failed for task container");
	q->max_capacity = INIT_SIZE;
	q->size = 0;
	q->queue_id = 1;

	return q;
}

/* add taskent to task_queue */
void add(task_queue *q, task *t)
{
	DIE(q == NULL || t == NULL, "task_queue insert called with NULL parameter");
	/* check for full task_queue array and resize if necessary */
	if (q->size + 1 >= q->max_capacity) {
		resize_type type = DOUBLE;
		resize(q, type);
	}

	/* set insert id for priority check */
	t->queue_id = q->queue_id++;

	/* add task to task_queue container */
	q->tasks[q->size] = t;
	q->size++;

	/* check and redo heap property for task_queue */
	heapify_up(q);

	/* set task info */
	t->state = READY;
	t->time_done = 0;
}

/* get task at the head of the task_queue - NULL if empty */
task *peek(task_queue *q)
{
	DIE(q == NULL, "task_queue peek called with NULL parameter");
	/* check for empty task_queue */
	if (q->size == 0)
		return NULL;
	return q->tasks[0];
}

/* get and remove task at the root of the task_queue - NULL if empty */
task *pop(task_queue *q)
{
	DIE(q == NULL, "task_queue pop called with NULL parameter");
	/* check for empty task_queue */
	if (q->size == 0)
		return NULL;

	/* remove task */
	/* put last element in container as root and heapify */
	task *t = q->tasks[0];
	q->size--;
	q->tasks[0] = q->tasks[q->size];
	heapify_down(q);

	/* resize task_queue if necessary */
	if (q->size * 4 <= q->max_capacity) {
		resize_type type = HALF;
		resize(q, type);
	}

	t->state = RUNNING;

	return t;
}

/* task_queue destructor */
void destruct_task_queue(task_queue *q)
{
	DIE(q == NULL, "task_queue destruct called with NULL parameter");

	free(q->tasks);
	free(q);
	q = NULL;
}
