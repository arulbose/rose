/* Rose RT-Kernel
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

#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

enum POOL_TYPE {

        MUTEX_POOL,
        SEMAPHORE_POOL,
        TIMER_POOL,
        QUEUE_POOL,
	EVENT_POOL,
	SHIRQ_POOL,

};

/* Internal functions */
void * __alloc_pool(int type);
void __free_pool(void *ptr, int type);

#endif
