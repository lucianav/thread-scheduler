/* Luciana Viziru - 332CA
 *
 * Threads scheduler
 *
 * 2016, Operating Systems
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "so_scheduler.h"
#include "linkedlist.h"
#include "task_queue.h"
#include "util.h"

typedef struct scheduler {
	task_queue *ready_queue; /* priority queue for tasks in ready state */
	list **waiting_lists;	/* lists used for threads in waiting state*/
	task *running_task;		/* reference to running task */
	unsigned max_events;	/* maximum number of events */
	unsigned quantum;		/* scheduler quantum */
	unsigned done;			/* variable used as completion */
							/* flag for condition */
	pthread_key_t mutex_key;	/* thread specific data key */
	pthread_cond_t cond_done;	/* condition for completion */
	pthread_mutex_t cond_mutex;	/* mutex used for completion condition */
} scheduler;

static scheduler *sched;	/* scheduler info struct */

int so_init(unsigned quantum, unsigned max_events)
{
	int ret, i;
	/* check for invalid init calls */
	if (sched != NULL || max_events > SO_MAX_NUM_EVENTS || quantum == 0)
		return -1;

	/* alloc scheduler structure */
	sched = malloc(sizeof(scheduler));
	/* so_init failed because of malloc */
	if (sched == NULL)
		return -1;

	/* initialize scheduler structure */
	sched->ready_queue = init_task_queue();
	sched->waiting_lists = malloc(max_events * sizeof(list *));
	for (i = 0; i < max_events; i++) {
		sched->waiting_lists[i] = NULL;
	}
	sched->running_task = NULL;
	sched->max_events = max_events;
	sched->quantum = quantum;
	sched->done = 0;
	ret = pthread_key_create(&(sched->mutex_key), NULL);
	DIE(ret != 0, "pthread_key_create failed");
	ret = pthread_mutex_init(&(sched->cond_mutex), NULL);
	DIE(ret != 0, "mutex init failed");
	ret = pthread_cond_init(&(sched->cond_done), NULL);
	DIE(ret != 0, "condition init failed");

	return 0;
}

static void schedule(void)
{
	task *tsk;
	int ret;

	/* get next task in ready queue */
	tsk = peek(sched->ready_queue);

	/* if running task is done, take next task and unblock*/
	if (sched->running_task->state == TERMINATED) {
		/* destroy task mutex member */
		ret = pthread_mutex_destroy(&(sched->running_task->thread_mutex));
		DIE(ret != 0, "thread mutex destroy failed");
		free(sched->running_task);

		if (tsk != NULL) {
			/* take next task and unblock */
			sched->running_task = pop(sched->ready_queue);
			ret = pthread_mutex_unlock(&(sched->running_task->thread_mutex));
			DIE(ret != 0, "schedule terminated unlock failed");
		}
		else {
			/* no other tasks in queue, set condition variable */
			ret = pthread_mutex_lock(&(sched->cond_mutex));
			DIE(ret != 0, "done cond mutex lock failed");
			sched->done = 1;
			ret = pthread_cond_signal(&(sched->cond_done));
			DIE(ret != 0, "pthread_cond_signal failed");
			ret = pthread_mutex_unlock(&(sched->cond_mutex));
			DIE(ret != 0, "done cond mutex unlock failed");
		}
		return;
	}

	/* only scheduler threads get here, increment time done */
	sched->running_task->time_done++;

	/* task eneters waiting state, block and take task from ready queue */
	if (sched->running_task->state == WAITING) {
		ret = pthread_mutex_lock(&(sched->running_task->thread_mutex));
		DIE(ret != 0, "schedule preempt lock failed");
		tsk = pop(sched->ready_queue);

		/* if there is a ready task, unblock */
		if (tsk) {
			sched->running_task = tsk;
			ret = pthread_mutex_unlock(&(sched->running_task->thread_mutex));
			DIE(ret != 0, "schedule preempt unlock failed");
		}
		return;
	}

	/* check if running task should be preempted */

	/* if running task has priority, it remains running */
	if (tsk == NULL || (sched->running_task->priority > tsk->priority)) {
		/* if time quantum reached, just reset time_done */
		if (sched->running_task->time_done == sched->quantum) {
			sched->running_task->time_done = 0;
		}
		return;
	}
	/* if both running and ready tasks have the same priority */
	/* and running task has not reached quantum, it remains running */
	if ((sched->running_task->priority == tsk->priority) &&
			(sched->running_task->time_done < sched->quantum)) {
		return;
	}

	/* any other case, running task is preempted */
	/* block, add in ready queue and unblock new task */
	ret = pthread_mutex_lock(&(sched->running_task->thread_mutex));
	DIE(ret != 0, "schedule preempt lock failed");
	tsk = pop(sched->ready_queue);
	add(sched->ready_queue, sched->running_task);
	sched->running_task = tsk;
	ret = pthread_mutex_unlock(&(sched->running_task->thread_mutex));
	DIE(ret != 0, "schedule preempt unlock failed");
}

static void *thread_start(void *task_info)
{
	int ret;
	/* extract task information */
	task *tsk = (task *)task_info;

	/* set thread mutex address as TSD value */
	pthread_mutex_t *mutex_addr = &(tsk->thread_mutex);
	ret = pthread_setspecific(sched->mutex_key, mutex_addr);
	DIE(ret != 0, "pthread_setspecific failed");

	/* wait for thread mutex to be unlocked */
	ret = pthread_mutex_lock(mutex_addr);
	DIE(ret != 0, "thread_start mutex lock failed");
	ret = pthread_mutex_unlock(mutex_addr);
	DIE(ret != 0, "thread_start mutex unlock failed");

	/* call handler */
	tsk->handler(tsk->priority);

	/* thread finished */
	tsk->state = TERMINATED;

	/* schedule next thread in queue */
	schedule();

	return NULL;
}

tid_t so_fork(so_handler *handler, unsigned priority)
{
	int ret;
	tid_t th;
	task *tsk;

	/* check for invalid parameters */
	if (priority > SO_MAX_PRIO || handler == NULL)
		return INVALID_TID;

	/* alloc task struct and initialize struct info */
	tsk = malloc(sizeof(task));
	DIE(tsk == NULL, "malloc failed");

	/* initialize thread blocking mutex */
	ret = pthread_mutex_init(&(tsk->thread_mutex), NULL);
	DIE(ret != 0, "mutex init failed");

	/* initialize task info */
	tsk->state  = NEW;
	tsk->time_done = 0;
	tsk->handler = handler;
	tsk->priority = priority;
	tsk->queue_id = 0;

	/* if scheduler thread is executing fork */
	if (sched->running_task != NULL) {
		/* block new thread before it starts running */
		ret = pthread_mutex_lock(&(tsk->thread_mutex));
		DIE(ret != 0, "fork initial lock failed");
	}

	/* create thread */
	ret = pthread_create(&th, NULL, &thread_start, tsk);
	DIE(ret != 0, "create thread failed");

	tsk->thread = th;

	/* if first thread in scheduler, set as running and unlock */
	if (sched->running_task == NULL) {
		tsk->state = RUNNING;
		sched->running_task = tsk;
	}
	else {
		/* add task in queue */
		/* run scheduler algorithm */
		add(sched->ready_queue, tsk);
		schedule();

		pthread_mutex_t *mutex_addr = pthread_getspecific(sched->mutex_key);
		/* thread belongs to scheduler, block if necessary */
		ret = pthread_mutex_lock(mutex_addr);
		DIE(ret != 0, "fork mutex lock failed");
		ret = pthread_mutex_unlock(mutex_addr);
		DIE(ret != 0, "fork mutex unlock failed");
	}

	return tsk->thread;
}

int so_wait(unsigned event_id)
{
	int ret;
	/* check for ivalid parameter */
	if (event_id >= sched->max_events) {
		return -1;
	}

	/* if event waiting list needs to, it is created */
	if (!sched->waiting_lists[event_id]) {
		sched->waiting_lists[event_id] = create_list();
	}

	/* set task as waiting and add to waiting list */
	sched->running_task->state = WAITING;
	insert(sched->waiting_lists[event_id], sched->running_task);

	/* run scheduling algorithm */
	schedule();

	pthread_mutex_t *mutex_addr = pthread_getspecific(sched->mutex_key);
	/* thread belongs to scheduler, block if necessary */
	/* it is guarateed there is always a thread to be scheduled */
	ret = pthread_mutex_lock(mutex_addr);
	DIE(ret != 0, "exec mutex lock failed");
	ret = pthread_mutex_unlock(mutex_addr);
	DIE(ret != 0, "exec mutex unlock failed");

	return 0;
}

int so_signal(unsigned event_id)
{
	int ret, nr_waiting, i;
	/* check for invalid parameter */
	if (event_id >= sched->max_events) {
		return -1;
	}

	/* get number of waiting threads for event */
	nr_waiting = get_size(sched->waiting_lists[event_id]);

	/* if no threads were waiting, return 0 */
	if (nr_waiting <= 0) {
		return 0;
	}

	/* get references to waiting tasks */
	task **ts = (task **)get_values(sched->waiting_lists[event_id]);

	/* wake waiting tasks and add to ready queue */
	for (i = 0; i < nr_waiting; i++) {
		add(sched->ready_queue, ts[i]);
	}

	/* free auxiliary memory */
	free(ts);
	/* clear all entries in list */
	empty_list(sched->waiting_lists[event_id]);

	/* run scheduling algorithm */
	schedule();

	pthread_mutex_t *mutex_addr = pthread_getspecific(sched->mutex_key);
	/* thread belongs to scheduler, block if necessary */
	/* it is guarateed there is always a thread to be scheduled */
	ret = pthread_mutex_lock(mutex_addr);
	DIE(ret != 0, "exec mutex lock failed");
	ret = pthread_mutex_unlock(mutex_addr);
	DIE(ret != 0, "exec mutex unlock failed");

	return nr_waiting;
}

void so_exec(void)
{
	int ret;

	/* run scheduling algorithm */
	schedule();

	pthread_mutex_t *mutex_addr = pthread_getspecific(sched->mutex_key);
	/* thread belongs to scheduler, block if necessary */
	ret = pthread_mutex_lock(mutex_addr);
	DIE(ret != 0, "exec mutex lock failed");
	ret = pthread_mutex_unlock(mutex_addr);
	DIE(ret != 0, "exec mutex unlock failed");
}

void so_end(void)
{
	int ret, i;
	task *t;

	/* check for invalid end call */
	if (sched == NULL)
		return;

	/* lock condition mutex */
	ret = pthread_mutex_lock(&(sched->cond_mutex));
	DIE(ret != 0, "done cond mutex lock failed");

	/* if there were tasks, wait for all tasks in scheduler */
	if (sched->running_task != NULL) {
		while (!sched->done) {
			pthread_cond_wait(&(sched->cond_done), &(sched->cond_mutex));
		}
	}

	ret = pthread_mutex_unlock(&(sched->cond_mutex));
	DIE(ret != 0, "done cond mutex unlock failed");

	/* free/destroy used memory/structures */
	destruct_task_queue(sched->ready_queue);

	for (i = 0; i < sched->max_events; i++) {
		/* if waiting list was created, destruct */
		if (sched->waiting_lists[i]) {
			destruct_list(sched->waiting_lists[i]);
		}
	}
	free(sched->waiting_lists);

	ret = pthread_key_delete(sched->mutex_key);
	DIE(ret != 0, "pthread_key_delete failed");

	ret = pthread_cond_destroy(&(sched->cond_done));
	DIE(ret != 0, "cond destroy failed");

	ret = pthread_mutex_destroy(&(sched->cond_mutex));
	DIE(ret != 0, "mutex destroy failed");

	free(sched);
	sched = NULL;
}

