/**********************************************************************
 * Copyright (c) 2019-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"
#define MAX_LIFESPAN (int)99999999999
/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;

/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];

/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;

/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;

struct resource_schedule
{
	int resource_id;
	int at;
	int duration;
	struct list_head list;
};
/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;
		//release를 하니까 프로세스의 상태를 다시 레디로 바꾸어주는 것.
		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue))
	{
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};

/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	struct process *next = NULL;
	// dump_status();

	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		return current;
	}
pick_next:
	//여기서부터가 이제 중요하지
	if (!list_empty(&readyqueue))
	{
		struct process *temp = NULL; //다음꺼로 선정한것.
		unsigned int minLife = MAX_LIFESPAN;
		//readyqueue에서 가장 작은 lifespan을 가진 process를 선정한다.
		list_for_each_entry(temp, &readyqueue, list)
		{
			if (minLife > temp->lifespan)
				minLife = temp->lifespan;
		}
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->lifespan == minLife)
			{
				list_del_init(&next->list);
				return next;
			}
		}
	}
	/**
	 * Implement your own SJF scheduler here.
	 */
	return NULL;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire,  /* Use the default FCFS acquire() */
	.release = fcfs_release,  /* Use the default FCFS release() */
	.schedule = sjf_schedule, /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};

/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
static struct process *srtf_schedule(void)
{
	struct process *next = NULL;
	struct process *temp = NULL;
	// dump_status();
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}
	/* The current process has remaining lifetime. Schedule it again */
	current->prio = current->lifespan - current->age;
	if (current->age < current->lifespan)
	{
		//srtf에서는 실행중인 부분에서 달라진다.
		//만약 실행중인것의 lifespan-age가 다른 lifespan 보다 낮다면?
		//lifespan-age가 priority라고 할 수 있다.
		list_for_each_entry(next, &readyqueue, list)
		{
			if (current->prio > next->prio)
			{
				//남아있는 시간이 srtf에서는 Priority가 된다!!
				list_add_tail(&(current->list), &readyqueue);
				temp = current;
				current = next;
				next = temp;
				//조건에 만족하는 next가 run될 current가 된다.
				//남아있는 시간이 더 적은 프로세스와 현재 프로세스를 바꾸어주어야한다.
				//readyqueue에 다시 넣어주어야한다.
				// list_add_tail(&(next->list), &readyqueue);
				list_del_init(&current->list);
			}
		}
		return current;
	}
pick_next:
	//여기서부터가 이제 중요하지
	if (!list_empty(&readyqueue))
	{
		struct process *temp = NULL;
		unsigned int minLife = MAX_LIFESPAN;
		//readyqueue에서 가장 작은 lifespan을 가진 process를 선정한다.
		list_for_each_entry(temp, &readyqueue, list)
		{
			if (minLife > temp->prio)
				minLife = temp->prio;
		}
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio == minLife)
			{
				list_del_init(&next->list);
				return next;
			}
		}
	}
	/**
	 * Implement your own SJF scheduler here.
	 */
	return NULL;
}
static void strf_forked(struct process *p)
{
	// list_add_tail(&p)
	p->prio = p->lifespan - p->age;
	// fprintf(stderr, "이건 테스트용 %d\n", p->prio);
}

struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.forked = strf_forked,
	.schedule = srtf_schedule /* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
							  /* Obviously, you should implement srtf_schedule() and attach it here */
};

/***********************************************************************
 * Round-robin scheduler
 * 1tick이 time quantum이다. 
 * 한프로세스가 한틱마다 readyque의 맨뒤로 그냥가면 된다. 
 * 기본은 fifo방식이다,
 ***********************************************************************/
static struct process *rr_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();
	struct process *temp = NULL;
	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		list_add_tail(&(current->list), &readyqueue);
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
		return next;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue))
	{
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule	 /* Obviously, you should implement rr_schedule() and attach it here */
};

/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
static struct process *prio_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();
	struct process *temp = NULL;
	struct process *temp2 = NULL;

	// fprintf(stderr, "test\n");
	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		list_add_tail(&(current->list), &readyqueue);

		temp = current;
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio > temp->prio)
			{
				temp = next;
			}
			else if (next->prio == temp->prio)
			{

				if (temp->prio > current->prio)
					continue; //이미 한번 바뀐거니까! 또바뀔이유 없지?
				temp = next;  //이렇게 하면 계속 똑같은게 나왔을 때 결국 마지막에는 current와 일치하게됨
				if (temp == current)
				{ //prio 가 있어도 동일한 Prio에 대해서는 rr 방식으로

					//반복문의 next(현재)와 current가 같다면?
					list_for_each_entry(temp2, &readyqueue, list)
					{
						if (temp2->prio == current->prio)
						{
							//처음으로 current와 같은것을 출력하는 것.
							temp = temp2;
							break;
						}
					}
				}
			}
			else
			{
				continue; //current는 변함 없으므로 temp에 넣어두자!
			}
		}
		//temp가 결국 최종적으로 나오게 될것.
		list_del_init(&temp->list);
		// list_del_init(&next->list);
		return temp;
	}

pick_next:
	/* Let's pick a new process to run next */
	if (!list_empty(&readyqueue))
	{
		struct process *old = NULL;
		struct process *temp = NULL;

		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_for_each_entry(old, &readyqueue, list)
		{
			if (next->prio < old->prio)
			{
				temp = next;
				next = old;
				old = temp;
			}
		}
		list_del_init(&next->list);
	}
	/* Return the next process to run */
	return next;
}
//이함수는 acquire가 발생할때!
bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		//아무도 리소스를 갖고 있지 않다면 owner가 되겠지?
		//current가 owner가 된다.
		r->owner = current;

		return true;
	}
	else
	{
		//resource의 owner가 존재하는 경우
		if (r->owner->pid != current->pid)
		{
			//owner가 존재하긴 하는데 current가 그 owner가 아니라면?
			//만약 누군가 리소스를 갖고있다면 이녀석은 waitqueue로 들어가고 current가 바뀌어야 맞겠지?
			//current는	readyque에 존재하지 않는다.
			/* OK, this resource is taken by @r->owner. */

			/* Update the current process state */

			current->status = PROCESS_WAIT;

			/** And append wait to waitqueue 
	 * resource를 가지고 있는 놈이 끝나기전까지 이 프로세스는 ready상태로 들어있을 수 없음.
	*/
			list_add_tail(&current->list, &r->waitqueue);
			//waitqueue에 넣고 새롭게 실행할 process를 찾는다.
			// current = prio_schedule();
			/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
			return false;
		}
		//current가 현재 자기 자신의 owner라면?
		return true;
	}
}
void prio_release(int resource_id)
{
	//release를 해줄 때 가장 높은 priority를 가지고 있는 waitqueue를 뽑아주어야 한다.
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *iterater;

		struct process *waiter =
			list_first_entry(&r->waitqueue, struct process, list);
		list_for_each_entry(iterater, &r->waitqueue, list)
		{
			//해당 resource를 wait하고 있는 것들중에 가장 Pior가 높은 것을
			//readyqueue로 변환하기 위한 작업.

			if (waiter->prio < iterater->prio)
			{
				//iterater와 temp의 값을 바꾼다.
				waiter = iterater;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;
		//release를 하니까 프로세스의 상태를 다시 레디로 바꾸어주는 것.
		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
		//wait하던게 readyqueue로 넘어오면서 다시 스케쥴링이 되게끔!!
	}
}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = prio_schedule
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};

/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/
static struct process *pa_schedule(void)
{
	struct process *next = NULL;
	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();
	struct process *temp = NULL;
	struct process *temp2 = NULL;
	// fprintf(stderr, "test\n");
	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		// list_add_tail(&(current->list), &readyqueue);
		// next = list_first_entry(&readyqueue, struct process, list);
		//우선 readyqueue 마지막에 넣어둔다.
		list_add_tail(&(current->list), &readyqueue);

		temp = current;
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio > temp->prio)
			{
				temp = next;
			}
			else if (next->prio == temp->prio)
			{

				if (temp->prio > current->prio)
					continue; //이미 한번 바뀐거니까! 또바뀔이유 없지?
				temp = next;  //이렇게 하면 계속 똑같은게 나왔을 때 결국 마지막에는 current와 일치하게됨
				if (temp == current)
				{ //prio 가 있어도 동일한 Prio에 대해서는 rr 방식으로

					//반복문의 next(현재)와 current가 같다면?
					list_for_each_entry(temp2, &readyqueue, list)
					{
						if (temp2->prio == current->prio)
						{
							//처음으로 current와 같은것을 출력하는 것.
							temp = temp2;
							break;
						}
					}
				}
			}
			else
			{
				continue; //current는 변함 없으므로 temp에 넣어두자!
			}
		}
		//temp가 결국 최종적으로 나오게 될것.
		list_del_init(&temp->list);
		temp->prio = temp->prio_orig;

		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio >= MAX_PRIO)
				continue;
			next->prio++;
		}
		return temp;
	}

pick_next:
	/* Let's pick a new process to run next */
	if (!list_empty(&readyqueue))
	{
		struct process *old = NULL;
		struct process *temp = NULL;

		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_for_each_entry(old, &readyqueue, list)
		{
			if (next->prio < old->prio)
			{
				temp = next;
				next = old;
				old = temp;
			}
		}
		next->prio = next->prio_orig;
		list_del_init(&next->list);

		list_for_each_entry(temp, &readyqueue, list)
		{
			temp->prio++;
		}
	}
	/* Return the next process to run */
	return next;
}
//이함수는 acquire가 발생할때!
bool pa_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		//아무도 리소스를 갖고 있지 않다면 owner가 되겠지?
		//current가 owner가 된다.
		r->owner = current;

		return true;
	}
	else
	{
		//resource의 owner가 존재하는 경우
		if (r->owner->pid != current->pid)
		{
			//owner가 존재하긴 하는데 current가 그 owner가 아니라면?
			//만약 누군가 리소스를 갖고있다면 이녀석은 waitqueue로 들어가고 current가 바뀌어야 맞겠지?
			//current는	readyque에 존재하지 않는다.
			/* OK, this resource is taken by @r->owner. */

			/* Update the current process state */

			current->status = PROCESS_WAIT;

			/** And append wait to waitqueue 
	 * resource를 가지고 있는 놈이 끝나기전까지 이 프로세스는 ready상태로 들어있을 수 없음.
	*/
			list_add_tail(&current->list, &r->waitqueue);
			/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
			return false;
		}
		//current가 현재 자기 자신의 owner라면?
		return true;
	}
}
void pa_release(int resource_id)
{
	//release를 해줄 때 가장 높은 priority를 가지고 있는 waitqueue를 뽑아주어야 한다.
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *iterater;

		struct process *waiter =
			list_first_entry(&r->waitqueue, struct process, list);
		list_for_each_entry(iterater, &r->waitqueue, list)
		{
			//해당 resource를 wait하고 있는 것들중에 가장 Pior가 높은 것을
			//readyqueue로 변환하기 위한 작업.
			if (waiter->prio < iterater->prio)
			{
				//iterater와 temp의 값을 바꾼다.
				waiter = iterater;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;
		//release를 하니까 프로세스의 상태를 다시 레디로 바꾸어주는 것.
		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
		//wait하던게 readyqueue로 넘어오면서 다시 스케쥴링이 되게끔!!
	}
}

struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = pa_acquire,
	.release = pa_release,
	.schedule = pa_schedule
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own pa_schedule() and attach it here */
};

/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
static struct process *pcp_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();
	struct process *temp = NULL;
	struct process *temp2 = NULL;

	// fprintf(stderr, "test\n");
	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		list_add_tail(&(current->list), &readyqueue);

		temp = current;
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio > temp->prio)
			{
				temp = next;
			}
			else if (next->prio == temp->prio)
			{

				if (temp->prio > current->prio)
					continue; //이미 한번 바뀐거니까! 또바뀔이유 없지?
				temp = next;  //이렇게 하면 계속 똑같은게 나왔을 때 결국 마지막에는 current와 일치하게됨
				if (temp == current)
				{ //prio 가 있어도 동일한 Prio에 대해서는 rr 방식으로

					//반복문의 next(현재)와 current가 같다면?
					list_for_each_entry(temp2, &readyqueue, list)
					{
						if (temp2->prio == current->prio)
						{
							//처음으로 current와 같은것을 출력하는 것.
							temp = temp2;
							break;
						}
					}
				}
			}
			else
			{
				continue; //current는 변함 없으므로 temp에 넣어두자!
			}
		}
		//temp가 결국 최종적으로 나오게 될것.
		list_del_init(&temp->list);
		// list_del_init(&next->list);
		return temp;
	}

pick_next:
	/* Let's pick a new process to run next */
	if (!list_empty(&readyqueue))
	{
		struct process *old = NULL;
		struct process *temp = NULL;

		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_for_each_entry(old, &readyqueue, list)
		{
			if (next->prio < old->prio)
			{
				temp = next;
				next = old;
				old = temp;
			}
		}
		list_del_init(&next->list);
	}
	/* Return the next process to run */
	return next;
}
//이함수는 acquire가 발생할때!
bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		//아무도 리소스를 갖고 있지 않다면 owner가 되겠지?
		//current가 owner가 된다.
		r->owner = current;

		r->owner->prio = MAX_PRIO;
		return true;
	}
	else
	{
		//resource의 owner가 존재하는 경우
		if (r->owner->pid != current->pid)
		{
			//owner가 존재하긴 하는데 current가 그 owner가 아니라면?
			//만약 누군가 리소스를 갖고있다면 이녀석은 waitqueue로 들어가고 current가 바뀌어야 맞겠지?
			//current는	readyque에 존재하지 않는다.
			/* OK, this resource is taken by @r->owner. */

			/* Update the current process state */

			current->status = PROCESS_WAIT;

			/** And append wait to waitqueue 
	 * resource를 가지고 있는 놈이 끝나기전까지 이 프로세스는 ready상태로 들어있을 수 없음.
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
			return false;
		}
		//current가 현재 자기 자신의 owner라면?
		return true;
	}
}
void pcp_release(int resource_id)
{
	//release를 해줄 때 가장 높은 priority를 가지고 있는 waitqueue를 뽑아주어야 한다.
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner->prio = r->owner->prio_orig;
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *iterater;

		struct process *waiter =
			list_first_entry(&r->waitqueue, struct process, list);
		list_for_each_entry(iterater, &r->waitqueue, list)
		{
			//해당 resource를 wait하고 있는 것들중에 가장 Pior가 높은 것을
			//readyqueue로 변환하기 위한 작업.
			if (waiter->prio < iterater->prio)
			{
				//iterater와 temp의 값을 바꾼다.
				waiter = iterater;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;
		//release를 하니까 프로세스의 상태를 다시 레디로 바꾸어주는 것.
		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
		//wait하던게 readyqueue로 넘어오면서 다시 스케쥴링이 되게끔!!
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire,
	.release = pcp_release,
	.schedule = pcp_schedule
	/**
	 * Implement your own acqure/release function too to make priority
	 * scheduler correct.
	 */
};

/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
static struct process *pip_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();
	struct process *temp = NULL;
	struct process *temp2 = NULL;

	// fprintf(stderr, "test\n");
	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT)
	{ //현재 프로세스가 없거나 프로세스의 상태가 wait이면 pick_next로가라.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		list_add_tail(&(current->list), &readyqueue);

		temp = current;
		list_for_each_entry(next, &readyqueue, list)
		{
			if (next->prio > temp->prio)
			{
				temp = next;
			}
			else if (next->prio == temp->prio)
			{

				if (temp->prio > current->prio)
					continue; //이미 한번 바뀐거니까! 또바뀔이유 없지?
				temp = next;  //이렇게 하면 계속 똑같은게 나왔을 때 결국 마지막에는 current와 일치하게됨
				if (temp == current)
				{ //prio 가 있어도 동일한 Prio에 대해서는 rr 방식으로

					//반복문의 next(현재)와 current가 같다면?
					list_for_each_entry(temp2, &readyqueue, list)
					{
						if (temp2->prio == current->prio)
						{
							//처음으로 current와 같은것을 출력하는 것.
							temp = temp2;
							break;
						}
					}
				}
			}
			else
			{
				continue; //current는 변함 없으므로 temp에 넣어두자!
			}
		}
		//temp가 결국 최종적으로 나오게 될것.
		list_del_init(&temp->list);
		// list_del_init(&next->list);
		return temp;
	}

pick_next:
	/* Let's pick a new process to run next */
	if (!list_empty(&readyqueue))
	{
		struct process *old = NULL;
		struct process *temp = NULL;

		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_for_each_entry(old, &readyqueue, list)
		{
			if (next->prio < old->prio)
			{
				temp = next;
				next = old;
				old = temp;
			}
		}
		list_del_init(&next->list);
	}
	/* Return the next process to run */
	return next;
}

//이함수는 acquire가 발생할때!
bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		//아무도 리소스를 갖고 있지 않다면 owner가 되겠지?
		//current가 owner가 된다.
		r->owner = current;

		return true;
	}
	else
	{
		//resource의 owner가 존재하는 경우
		if (r->owner->pid != current->pid)
		{
			//owner가 존재하긴 하는데 current가 그 owner가 아니라면?
			//만약 누군가 리소스를 갖고있다면 이녀석은 waitqueue로 들어가고 current가 바뀌어야 맞겠지?
			//current는	readyque에 존재하지 않는다.
			/* OK, this resource is taken by @r->owner. */

			/* Update the current process state */

			current->status = PROCESS_WAIT;

			/** And append wait to waitqueue 
	 * resource를 가지고 있는 놈이 끝나기전까지 이 프로세스는 ready상태로 들어있을 수 없음.
	*/
			list_add_tail(&current->list, &r->waitqueue);
			if (current->prio > r->owner->prio)
			{
				r->owner->prio = current->prio + 1;
			}
			/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
			return false;
		}
		//current가 현재 자기 자신의 owner라면?
		return true;
	}
}
void pip_release(int resource_id)
{
	//release를 해줄 때 가장 높은 priority를 가지고 있는 waitqueue를 뽑아주어야 한다.
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner->prio = r->owner->prio_orig;
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *iterater;

		struct process *waiter =
			list_first_entry(&r->waitqueue, struct process, list);
		list_for_each_entry(iterater, &r->waitqueue, list)
		{
			//해당 resource를 wait하고 있는 것들중에 가장 Pior가 높은 것을
			//readyqueue로 변환하기 위한 작업.
			if (waiter->prio < iterater->prio)
			{
				//iterater와 temp의 값을 바꾼다.
				waiter = iterater;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;
		//release를 하니까 프로세스의 상태를 다시 레디로 바꾸어주는 것.
		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
		//wait하던게 readyqueue로 넘어오면서 다시 스케쥴링이 되게끔!!
	}
}

struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire,
	.release = pip_release,
	.schedule = pip_schedule
	/**
	 * Ditto
	 */
};
