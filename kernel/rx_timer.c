/* RosX RT-Kernel
 * Copyright (C) 2016 Arul Bose<bose.arul@gmail.com>
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

#include <RosX.h>

RX_DEFINE_EVENTGROUP(__rx_timer_events);

int __rx_timer_tick_irq_handler(int irq, void *a)
{
	/* Wake up timer thread and return */
	//pr_dbg("timer interrupt received\n");
	jiffies ++;
	rx_notify_event(&__rx_timer_events, RX_TIMER_EVENT_IRQ_EVENT);

#ifdef CONFIG_TIME_SLICE
    /* Check if time slice is enabled */
        if(__rx_curr_running_task->time_slice > 0) {
             __rx_curr_running_task->ticks--;
             if(!__rx_curr_running_task->ticks) { /* once timer expires; sched */
                /* re-arrange the ready queue so that same prio process will not be starved */
                __rx_remove_from_ready_q(__rx_curr_running_task);
                __rx_curr_running_task->state = RX_TASK_READY; /* RX_TASK_RUNNING->RX_TASK_READY */
                __rx_add_to_ready_q(__rx_curr_running_task);
		if(__rx_curr_running_task != __rx_task_ready_head) { /* check if pre-emption is needed */
                    __rx_need_resched = 1;
		}else{
			__rx_curr_running_task->ticks = __rx_curr_running_task->time_slice; /* No pre-emption required; reload the ticks */
			__rx_curr_running_task->state = RX_TASK_RUNNING;
		}
             }
        }
#endif

	return RX_IRQ_HANDLED;
}

/* Timer thread with system priority 0 */
void rx_timer_thread()
{
	unsigned int event_flag = 0;
	unsigned int imask;
	struct timer_list *start = NULL;

        unsigned int tick = 0;

        /* put the timer thread in the ready queue; system thread priority */
	pr_dbg( "In rosx_timer_thread\n");

	/* request timer interrupt */
	if(OS_OK != rx_request_irq(RX_TIMER0_INT, &__rx_timer_tick_irq_handler, 0, "timer_irq", 0)) {
                pr_panic("RX_TIMER0_INT0 irq alloc failed\n");
       }
	/* Parse the timer list and wake up expired timers */
	while(1) {
		event_flag = rx_wait_event_group(&__rx_timer_events, RX_TIMER_EVENT_IRQ_EVENT); 
		//pr_dbg("jiffies %u\n", jiffies);
	
		imask = rx_enter_critical();	
		if ((event_flag & RX_TIMER_EVENT_IRQ_EVENT) == RX_TIMER_EVENT_IRQ_EVENT){
				
			rx_clear_event_flag(RX_TIMER_EVENT_IRQ_EVENT);
        		start = __rx_active_timer_head;
		        tick = jiffies;
			/* parse the timer list for expired timers */		
	   		while(start != NULL)
           		{	
				/* Expired timers are always in the start of the active list  */
                		if(start->time_expiry <= tick)
                		{
					if(start->handler != NULL ) {
						/* remove from active list to idle list */
						__rx_remove_from_timer_list(start, &__rx_active_timer_head);
						start->flag &= __RX_TIMER_DISABLED;
						/* Handler should be quick to finish its job */
						start->handler(start->priv);
					}else{
						/* In case of non handler type calls like msleep() or wait on timeout */
                        			start->task->state = RX_TASK_READY;
                                                __rx_add_to_ready_q(start->task);
						start->task->timer = NULL;
						__rx_remove_from_timer_list(start, &__rx_active_timer_head);
					}
					start = __rx_active_timer_head;/** Reset the timer head as new timer may be added by rx_mod_timer call **/
				} else {
                		    break; /* No expired timers in the active list */
				}
        		}

      		}else{
			pr_error( "wrong event notification\n");
      		}
			rx_exit_critical(imask);
	}
}

void rx_ndelay()
{
	/* coarse loop calculation using bogus mips else for RT then use precision HW timer */
}

void rx_udelay()
{
	/* coarse loop calculation using bogus mips else for RT then use precision HW timer */

}

void rx_mdelay()
{
	/* coarse loop calculation using bogus mips else for RT then use precision HW timer */
	
}

/* The minimum limit is CONFIG_HZ; resoultion is limited to CONFIG_HZ */
int rx_msleep(unsigned int time)
{
    struct timer_list timer;

    if(time > 100000 || time == 0) { /* add to smoothen the time to the next system tick */
	pr_error("smrx_ssleep: check params\n");
	return OS_ERR;
    }

    /* add code for jiffy rollover */
    time = time/CONFIG_HZ; /* convert time to ticks and add the jiffies for the future  */
    if(!time)
 	time = 1; /* The resolution of the ms timer is CONFIG_HZ */

    /* Add to the timer list with the least timer in the front */
    __rx_add_timer(&timer, NULL, time, __rx_curr_running_task);
    return OS_OK;
}

/* Seconds timer which gets waked up when timer expires */
int rx_ssleep(unsigned int time)
{
  struct timer_list timer;

  if(time == 0 ) {
	pr_error("rx_ssleep: check params\n");
                return OS_ERR;
  }

   time = (time * 1000/CONFIG_HZ); /* convert time to ticks */
  /* add code for jiffy rollover */
  /* Add to the timer list with the least timer in the front */
   __rx_add_timer(&timer, NULL, time, __rx_curr_running_task);
   return OS_OK;
}

/* Add the newly created timer in timer list
 *
 * */
int __rx_add_timer(struct timer_list *p, void (*func)(void *), int delay, RX_TASK *tid)
{
       p->delay = delay;
       p->time_expiry = 0;
       p->task = tid;
       p->handler = func;
       p->next = NULL;
       p->prev = NULL;
	
	if(func == NULL){
		__rx_add_to_active_timer_list(p);
		tid->timer = p; /* Will help to know if task is waiting on a timer */
		/* Sleep on the timer list; until woken up by timer thread  */
		tid->state = RX_TASK_SUSPEND;
                __rx_remove_from_ready_q(tid);
		rx_sched();
	}else{
		p->flag &= __RX_TIMER_DISABLED;
        }

	return OS_OK;
}

/* Static init of timers */
int rx_init_timer(struct timer_list *timer, void (*timer_handler)(void *), void *priv, unsigned int ticks)
{
	if(ticks == 0){
                pr_error( "rx_create_timer: Check params\n");
                return OS_ERR;
        }

	timer->priv = priv;
	timer->flag = 0;

        __rx_add_timer(timer, timer_handler, ticks, __rx_curr_running_task);
	return OS_OK;
}

int rx_create_timer(struct timer_list *timer, void (*timer_handler)(void *), void *priv, unsigned int ticks)
{
	if(ticks == 0){
		pr_error( "rx_create_timer: Check params\n");
		return OS_ERR;
	}	

	timer->priv = priv;
	__rx_add_timer(timer, timer_handler, ticks, __rx_curr_running_task);			
	return OS_OK;
}

void rx_delete_timer(struct timer_list *timer)
{
	if(timer->flag & __RX_TIMER_ENABLED){
		pr_error( "delete_timer: first stop the timer to delete %s\n", timer->task->name);
		return;
	}
}

/* mod timer will modifiy the delay;  and start the time r */
void rx_mod_timer(struct timer_list *p, unsigned int delay)
{
	if(!p) {
		pr_error( "rx_mod_timer: timer is NULL\n");
		return;
	}

	if(p->flag & __RX_TIMER_ENABLED) {
		 pr_error( "rx_mod_timer: Timer is active; call rx_stop_timer  TID %s\n", p->task->name);
		 return;
	}
	/* modify the value and start the timer */
	if(p->delay == 0) {
		pr_dbg( "rx_mod_timer: check params\n");
		return;
	}
        p->delay = (delay * (1000/CONFIG_HZ)); /* convert to ticks */
	rx_start_timer(p);
}

/* Start_timer has to be called to enable the timer
 * It adds the time rto active timer list; for cyclic operation rx_mod_timer has to be called */
void rx_start_timer(struct timer_list *p)
{
	if(!p) {
                pr_error( "rx_start_timer: timer is NULL\n");
                return;
        }

	if(p->flag & __RX_TIMER_ENABLED) {
		pr_error( "rx_start_timer: Timer already started TID %s\n", p->task->name);
	} else {
		__rx_add_to_active_timer_list(p);
	}
}

/* rx_stop_timer should be called to disable the timer 
 * from then it will be not added to active timer list 
 * until t is re-enabled by rx_start_timer */
void rx_stop_timer(struct timer_list *p)
{
	if(p->flag & __RX_TIMER_ENABLED) {
        		__rx_remove_from_timer_list(p, &__rx_active_timer_head);
			p->flag &= __RX_TIMER_DISABLED;
	} else {
		pr_error( "rx_stop_timer: Timer already stopped TID %s\n", p->task->name);
	}
}

/* Always add the timer with the short expiry to the start of the active list */ 
void __rx_add_to_active_timer_list(struct timer_list *p)
{
	struct timer_list *start = NULL;
	unsigned int tick;
	unsigned int imask;

	imask = rx_enter_critical();

	p->time_expiry = p->delay + jiffies; /* push the time to future */
	p->flag |= __RX_TIMER_ENABLED;
	tick = p->time_expiry; 
	p->prev = NULL;
	p->next = NULL;

	/* If the list is empty */
	if(!__rx_active_timer_head) { /* Case 1: If list is empty*/
		__rx_active_timer_head = p;
		rx_exit_critical(imask);
		return;		
	}

	/* add the timer in the active timer list */
	start = __rx_active_timer_head;

	 while(start != NULL) {

                /* Always head will have low timer values */
                if(start->time_expiry >= tick) { 
                        if(start == __rx_active_timer_head){ /* Case 2: In case the new timer is added before the timer head */
                                /* Insert the new timer as head */
                                __rx_active_timer_head->prev = p;
                                p->next = __rx_active_timer_head;
                                __rx_active_timer_head = p;
				rx_exit_critical(imask);
				return; /* Done inserting in the active list */
                        }else{
                                /* Case 3: In case timer is less in the middle */
                                p->next = start;
				start->prev->next = p;
                                p->prev = start->prev;
				start->prev = p;
				rx_exit_critical(imask);
				return;
                        }
               }else {
		if(start->next == NULL)
			break;
                start = start->next;
	       }
	}
		/* Add the timer to the end of the list */		
         start->next = p;
	 p->prev = start;
	 rx_exit_critical(imask);
}

/* Used to remove a timer from idle or active timer list */
void __rx_remove_from_timer_list(struct timer_list *p, struct timer_list **head)
{
    unsigned int imask;

    imask = rx_enter_critical();

    if(*head == NULL){
	pr_error( "Timer error __rx_remove_from_timer_list\n"); /* change with panic */
	rx_exit_critical(imask);
	return;
    }

     if(*head == p) {
         if(p->next != NULL){
             *head = p->next; /* move the active timer node */
	    (*head)->prev = NULL;
            } else {
                        *head = NULL; /* timer list becomes empty after reoving */
	    }
     } else {
            /*remove the timer from the active list */
            p->prev->next = p->next;
            p->next->prev = p->prev;
     }

    rx_exit_critical(imask);
}

inline int  RX_MSECS_TO_TICKS(int m)
{
    int __ret = 0;
    if(!m)
       return  __ret;
    if((m) < (1000/CONFIG_HZ)) {
        return 1;
    }else{
        return ((m) / (1000/CONFIG_HZ));
    }
}

