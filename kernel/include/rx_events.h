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

#ifndef __EVENTS_H__
#define __EVENTS_H__

/********************** Events **********************/
struct event_group{
        RX_TASK *task; /* task waiting for the event */
        struct event_group *next;
};

#define RX_DEFINE_EVENTGROUP(event)    \
      struct event_group (event) = __EVENTGROUP_INIT(event)

#define __EVENTGROUP_INIT(event)    \
           {                     \
             .task = NULL,       \
             .next = NULL        \
           }

void rx_event_thread();

/* -------------- Application system calls ---------------- */
int rx_create_event_group(struct event_group *p);
void rx_delete_event_group(struct event_group *p);

int rx_set_event_flag(unsigned int flag);
int rx_clear_event_flag(unsigned int flag);
int rx_wait_event_group(struct event_group *, int flag);
void rx_notify_event(struct event_group *, unsigned int flag);

#endif /* __EVENTS_H__*/
