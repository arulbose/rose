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

/** create_task() - Create new task
 *  @tcb:		Task Control Block
 *  @task_name:		Name for the created task
 *  @prio:		Task priority
 *  @stack_ptr:		Stack pointer for the task. Can be NULL if CONFIG_STACK_ALLOC_DYNAMIC defined 
 *  @stack_size:	Size of the stack for the task
 *  @func:		Task entry function
 *  task_state:		Task state when created
 *  time_slice:		The time in ticks the process should run in case of task competing with same priority
 */
int rx_create_task(RX_TASK *tcb, char *task_name, int prio, void *stack_ptr, int stack_size, void (*func)(void), int task_state, int time_slice)
{
	RX_TASK *tmp;
	unsigned int imask;

	if(strlen(task_name) > RX_TASK_STR_LEN){
		pr_error( "%s %s\n", "Task name overflow", task_name);
		return OS_ERR;
	}

	imask = rx_enter_critical();

	#ifdef CONFIG_STACK_ALLOC_DYNAMIC
	/* if stack_ptr is NULL go for dynamic stack allocation */
	if(!stack_ptr) {

  		if(((char *)__rx_stack_start_ptr - CONFIG_SYSTEM_STACK_SIZE) > ((char *)__rx_curr_stack_ptr - stack_size)){
	        	pr_panic( "%s %s\n", "Stack size overflow\n", task_name);
			rx_exit_critical(imask);
			return OS_ERR;
		}
		stack_ptr = __rx_curr_stack_ptr;
		__rx_curr_stack_ptr = ((char *)__rx_curr_stack_ptr - (stack_size + 4));
	}
 	#endif

	strcpy((char *)tcb->name, task_name);	
	tcb->curr_stack_ptr = stack_ptr;
	tcb->stack_start_ptr = stack_ptr;
        if (prio > RX_TASK_LEAST_PRIO)
            prio = RX_TASK_LEAST_PRIO; /* Fix it to the RX_TASK_LEAST_PRIO */

	tcb->prio = prio;
#ifdef CONFIG_PRIO_INHERITANCE
	tcb->orig_prio = prio;
#endif
	tcb->stack_size = stack_size;
	tcb->func = func;
	tcb->state = task_state;
        tcb->timeout = __RX_TIMER_OFF;
#ifdef CONFIG_TIME_SLICE
	tcb->time_slice = time_slice;
	tcb->ticks = time_slice;
	tcb->timer = NULL;
#endif
	tcb->list = NULL;
	/* port specific initialiation <start>*/
	__rx_init_tcb__(tcb);	
	/* port specific initialiation <end>*/
	tcb->ip =  tcb->func;

	if(task_state == RX_TASK_READY)
		__rx_add_to_ready_q(tcb);

	/* add the task to the global list of tasks */
	if(!__rx_task_list_head){
                __rx_task_list_head = tcb;
        }else{
                tmp = __rx_task_list_head;
                while(tmp->list)
                        tmp = tmp->list;

                tmp->list = tcb;
        }

	__rx_curr_num_task ++;

	rx_exit_critical(imask);
	
	return OS_OK;
}

/* Sort based on task priority while adding to the ready list */
int __rx_add_to_ready_q(RX_TASK * new)
{
	RX_TASK *start = NULL;	
	RX_TASK *prev = NULL;	
	unsigned int imask;
	int prio = new->prio;

	if(!(new->state == RX_TASK_READY || new->state == RX_TASK_RUNNING) ){
	    pr_error( "__rx_add_to_ready_q: task is not in ready state %s, %d\n", new->name, new->state);
	    return OS_ERR;
	}

	new->next = NULL;

	imask = rx_enter_critical();
	if(!__rx_task_ready_head){ 
	    __rx_task_ready_head = new;
	    rx_exit_critical(imask);
	    return OS_OK;
	}
	
	start = __rx_task_ready_head;

	while(start) {	
		
		/* Always head will have the high priority task */
		if(start->prio >= prio) {
			if(start->prio == prio)
			{ /* Same priority task are always added at the after to behave in a co-operative way */
				if(start->next) {
				    if(start->next->prio != prio){ /* if next node has the same prio than move to the next */
				        new->next = start->next;
                                        start->next = new;
                                        goto done;
				    }

				}else{
					new->next = start->next;
					start->next = new;
					goto done;
				}
			}else{
				new->next = start;
				if(start == __rx_task_ready_head){
				    /* In case head prio is less */
			            __rx_task_ready_head = new;	
				    goto done;	
				}else{
				    /* In case prio is less in the middle */
				    prev->next = new;
				    goto done;
				}
		       }
		}
		prev = start;
		start = start->next;
	}

	/* In case the node should be added at the end */
	prev->next = new; 

done:
     if(__rx_curr_running_task != __rx_task_ready_head){
           __rx_need_resched = 1; /* Used for pre-emptive re-scheduling */
     }
        rx_exit_critical(imask);
	return OS_OK;
}

/* remove from ready runqueue  */
int __rx_remove_from_ready_q(RX_TASK * rmv)
{
	RX_TASK *start = NULL;	
	RX_TASK *prev = NULL;	
	unsigned int imask;

	imask = rx_enter_critical();

 	start = __rx_task_ready_head;

        while(start != NULL) {

                /* Always head will have the high priority task */
                if(start == rmv ) {

                        if(start == __rx_task_ready_head){
                                /* In case head is the task */
                                __rx_task_ready_head = start->next;
				rmv->next = NULL;
				rx_exit_critical(imask);
				return OS_OK;
                        }else{
                                /* In case task is in the middle */
                                prev->next = start->next;
				rmv->next = NULL;
				rx_exit_critical(imask);
				return OS_OK;
                        }
		}
                prev = start;
                start = start->next;
       }
                /* In case task is at the end */
                prev->next = NULL;
		rmv->next = NULL;
		rx_exit_critical(imask);
		return OS_OK;
}


/* Task cannot abort itself but can be done by other threads 
 * Aborted tasked cannot be resumed */
int rx_abort_task(RX_TASK *tcb)
{
	if(tcb == __rx_curr_running_task)
		return OS_ERR; /* A task cannot abort itself */

	/* releive all the resource that the task curently holds*/
	if(tcb->state == RX_TASK_READY) {
		 __rx_remove_from_ready_q(tcb);
	}
	/* TODO Remove form any waitqueues/return resources  and then change the state to task_abort */
	tcb->state = RX_TASK_ABORT;

	return OS_OK;
}

/* Only self complete allowed; Always call this function to gracefully close the task 
 * App has to make sure it returns all the resources before calling task_complete */
int rx_complete_task(RX_TASK *tcb)
{
	if(tcb != __rx_curr_running_task)
		return OS_ERR;

	__rx_remove_from_ready_q(tcb);
	tcb->state = RX_TASK_COMPLETE;
	rx_sched();
	return OS_OK;
}

/* Set task priority at runtime */
int rx_set_task_prio(RX_TASK *tcb, int prio)
{
	if((prio == tcb->prio)|| tcb->prio == RX_TASK_COMPLETE || tcb->prio == RX_TASK_ABORT)
		return 0; /* Nothing to do */

	tcb->prio = prio;
	if(tcb->state == RX_TASK_READY) {
		__rx_remove_from_ready_q(tcb);
		__rx_add_to_ready_q(tcb);
	}
	return OS_OK;
}

/* Task should be either in ABORT or COMPLETE state before calling this function  */
int rx_delete_task(RX_TASK *tcb)
{
	RX_TASK *tmp;

	if((tcb->state == RX_TASK_SUSPEND) || (tcb->state == RX_TASK_READY))
		return OS_ERR;

	/* remove from the global task list */
	if(tcb == __rx_task_list_head){
                        __rx_task_list_head = tcb->list;
        } else {
                        tmp = __rx_task_list_head;
                        while(tmp->list) {
                                if(tmp->list == tcb)
                                        break;
                                tmp = tmp->list;
                        }
                        tmp->list = tmp->list->list;
        }

	return OS_OK;
}

/* Task can be suspended by own or by other task. It has to be resumed by calling task_resume */
int rx_suspend_task(RX_TASK *tcb)
{
	if(!tcb) {

	    __rx_remove_from_ready_q(__rx_curr_running_task);
            __rx_curr_running_task->state = RX_TASK_SUSPEND;
            rx_sched();

	}else{
		if(tcb->state == RX_TASK_READY) {
		   __rx_remove_from_ready_q(tcb);
                   tcb->state = RX_TASK_SUSPEND;
		}
	}
	return OS_OK;
}

int rx_resume_task(RX_TASK *tcb)
{
	if(tcb->state != RX_TASK_SUSPEND)
		return OS_ERR;

	tcb->state = RX_TASK_READY;
	__rx_add_to_ready_q(tcb);

	return OS_OK;
}
