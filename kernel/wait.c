/* Rose RT-Kernel
 * Copyright (C) 2018 Arul Bose<bose.arul@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <RoseRTOS.h>

/* Rose Wait Q
 * */

static struct wait_queue *__sys_wait_list = NULL; /* Pointer to the list of tasks waiting on the wait queue */
static void __wait_timeout(void);
static void __wait_handler(void *ptr);
static void wake_task_in_the_queue(struct wait_queue **ride);

int __finish_wait()
{
    int ret = 0;

    unsigned int imask = enter_critical();
    /* Find the current task */

    if(__curr_running_task->timer != NULL) {
        /* Stop the timer and remove the timer from the list */
        __remove_from_timer_list(__curr_running_task->timer, &__active_timer_head);
        __curr_running_task->timer = NULL;    
    }
    if(__curr_running_task->timeout == E_OS_TIMEOUT) {
        ret = E_OS_TIMEOUT;
    }
    __curr_running_task->timeout = __TIMER_OFF;
    __curr_running_task->wq = NULL;

    exit_critical(imask);

    return ret;

}

static void __wait_timeout()
{
    struct timer_list *timer = NULL;

    if (!(timer = create_timer(__wait_handler, __curr_running_task,  __curr_running_task->timeout))){
        pr_panic("Timer creation failed\n");
    }
    __curr_running_task->timer = timer;
    start_timer(timer);
    __curr_running_task->timeout = __TIMER_ON;
}

/* Timer context */
static void __wait_handler(void *ptr)
{
    TCB *t = (TCB *)ptr;
    delete_timer(t->timer);
    t->timeout = E_OS_TIMEOUT;
    t->timer = NULL;
}

int __add_to_wait_queue(struct wait_queue *wq, int task_state, int timeout)
{
    struct wait_queue *ride;
    TCB *t;

    unsigned int imask = enter_critical();

   /* Put the current task to sleep in its waitqueue */    
    __curr_running_task->state = task_state;
    __curr_running_task->wq = wq;
    __remove_from_ready_q(__curr_running_task);
    __curr_running_task->next = NULL;
 
    /* Add the tasks in its wait_queue */ 
    if(!(wq->task)) {
        wq->task = __curr_running_task;
    }else{
        t = wq->task;
        while(t->next){
            t = t->next;
        }
        t->next = __curr_running_task;
    }
    /* Check if the waitqueue is already in the list */
        /* Now add the waitqueue to the sys wait queue */    
    if(!(__sys_wait_list)) {
            /* Empty; add the first node */
            __sys_wait_list = wq;
    }else{
        /* add wait queue to the tail of sytem wait list */
        ride = __sys_wait_list;
        while(ride->next){
            if(ride == wq){
                break; /* wq is already in the list */
            }
            ride = ride->next;
        }
        if(!(ride == wq)) { /* wq is already in the list */
            ride->next = wq;
            wq->prev = ride;
        }
   }
    /* Wait timeout;Avoid starting the timer is already started */
    if((timeout > 0) && (__curr_running_task->timeout == __TIMER_OFF)) {
        __curr_running_task->timeout = timeout;
        __wait_timeout();
    }

    exit_critical(imask);

    return 0;
}

/* Wakeup all the task waiting on the wait queue */
int wakeup(struct wait_queue *wq)
{
    TCB *ready;
  
    unsigned int imask = enter_critical();
    
    TCB *t = wq->task;
    
    while(t) {
        t->state = TASK_READY; 
        ready = t;  
        t = t->next;
        __add_to_ready_q(ready);
    }    
    wq->task = NULL;
    /* Remove the queue from the sys queue list */
    if(wq == __sys_wait_list) {
         __sys_wait_list = wq->next;
         if(__sys_wait_list) {
              __sys_wait_list->prev = NULL;
         }
    }else{
         /* Remove the task from the waitqueue */
         wq->prev->next = wq->next;
         wq->next->prev = wq->prev;
         wq->prev = wq->next = NULL;     
    }

    exit_critical(imask);

    return 0;
}

/* Wake all the task in this queue */
static void wake_task_in_the_queue(struct wait_queue **ride)
{
    TCB *tn;
    TCB *tp;
    TCB *ready;
    tn = tp = (*ride)->task;
    
    while(tn){
        if((tn->state == TASK_INTERRUPTIBLE) || \
           (tn->timeout == E_OS_TIMEOUT) ) {
            /* Wake up task with state TASK_INTERRUPTIBLE or E_OS_TIMEOUT */
            tn->state = TASK_READY;
            ready = tn;
            if(tn == (*ride)->task){
                /* Move the head of the queue */
                tn = tn->next;
                (*ride)->task = tn;
                tp = tn;
                __add_to_ready_q(ready);
           }else{
               /* Remove the task if in between the task chain */
               tn = tn->next;
               tp->next = tn;
               __add_to_ready_q(ready);
           }
                    
       }else { /* !TASK_INTERRUPTIBLE && !E_OS_TIMEOUT */
                    tp = tn;
                    tn = tn->next;
             }
    }

}

/* Runs in the context of rose_event_thread()
 * Wake all the tasks in a queue list; Will be called from event_group thread */

void __rose_wake()
{
    struct wait_queue *ride;
    struct wait_queue *wq;

    if(!__sys_wait_list)
        return;
    
    ride = __sys_wait_list;
    /* Walk through each queue in the queue list and wake up all the task
     * if TASK_INTERRUPTIBLE or E_OS_TIMEOUT
     */
    while(ride)
    {
        if(ride == __sys_wait_list) {
            wake_task_in_the_queue(&ride);  
            if(ride->task == NULL) {
                 wq = ride;
                /* If the task list is empty for the queue the move the head to the next one  */
                __sys_wait_list = ride->next;
                 wq->prev = wq->next = NULL;
                if(__sys_wait_list){
                    __sys_wait_list->prev = NULL;
                }
            }
        }else{ /* !(ride == __sys_wait_list) */
             wake_task_in_the_queue(&ride);  
             if(ride->task == NULL) {
                 wq = ride;
                 /* Remove the task from the waitqueue */ 
                 ride->prev->next = ride->next;
                 ride->next->prev = ride->prev; 
                 wq->prev = wq->next = NULL;
             }
       }
       ride = ride->next; /* move to the next wq */
    }
}
