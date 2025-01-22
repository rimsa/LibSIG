/*--------------------------------------------------------------------*/
/*--- LibSIG                                                       ---*/
/*---                                                    threads.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of LibSIG, a dynamic library signature tool.

   Copyright (C) 2025, Andrei Rimsa (andrei@cefetmg.br)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "global.h"

#include "pub_tool_threadstate.h"

static void exec_state_save(exec_state* state);
static void exec_state_restore(exec_state* state);

/*------------------------------------------------------------*/
/*--- Support for multi-threading                          ---*/
/*------------------------------------------------------------*/


/*
 * For Valgrind, MT is cooperative (no preemting in our code),
 * so we don't need locks...
 *
 * Per-thread data:
 *  - bound
 *  - records
 *
 * Even when ignoring MT, we need this functions to set up some
 * datastructures for the process (= Thread 1).
 */

/* current running thread */
ThreadId LSG_(current_tid);

static thread_info** threads;

thread_info** LSG_(get_threads)(void) {
	return threads;
}

thread_info* LSG_(get_current_thread)(void) {
	return threads[LSG_(current_tid)];
}

void LSG_(init_threads)(void) {
	UInt i;

	threads = LSG_MALLOC("lsg.threads.it.1", VG_N_THREADS * sizeof(threads[0]));

	for (i = 0; i < VG_N_THREADS; i++)
		threads[i] = 0;

	LSG_(current_tid) = VG_INVALID_THREADID;
}

/* switches through all threads and calls func */
void LSG_(forall_threads)(void (*func)(thread_info*)) {
	Int t;
	ThreadId orig_tid = LSG_(current_tid);

	for (t = 1; t < VG_N_THREADS; t++) {
		if (!threads[t])
			continue;

		LSG_(switch_thread)(t);
		(*func)(threads[t]);
	}

	LSG_(switch_thread)(orig_tid);
}


static
thread_info* new_thread(void) {
	thread_info* t;

	t = (thread_info*) LSG_MALLOC("lsg.threads.nt.1",
							sizeof(thread_info));

	t->state.bound = Nobound;
	t->state.records.head = 0;
	t->state.records.last = 0;

	return t;
}

static void delete_records(thread_info* t) {
	Record* record = t->state.records.head;
	while (record) {
		Record* tmp = record->next;
		LSG_DATA_FREE(record, sizeof(Record));
		record = tmp;
	}
}

static void delete_thread(thread_info* t) {
	LSG_ASSERT(t != 0);
	delete_records(t);
	LSG_DATA_FREE(t, sizeof(thread_info));
}

void LSG_(destroy_threads)(void) {
	UInt i;

	for (i = 0; i < VG_N_THREADS; i++) {
		if (threads[i]) {
			delete_thread(threads[i]);
			threads[i] = 0;
		}
	}

	LSG_FREE(threads);
	threads = 0;

	LSG_(current_tid) = VG_INVALID_THREADID;
}

void LSG_(switch_thread)(ThreadId tid) {
	if (tid == LSG_(current_tid))
		return;

	LSG_DEBUG(0, ">> thread %u (was %u)\n", tid, LSG_(current_tid));

	if (LSG_(current_tid) != VG_INVALID_THREADID) {
		/* save thread state */
		thread_info* t = threads[LSG_(current_tid)];
		LSG_ASSERT(t != 0);

		/* current context (including signal handler contexts) */
		exec_state_save(&(t->state));
	}

	LSG_(current_tid) = tid;
	LSG_ASSERT(tid < VG_N_THREADS);

	if (tid != VG_INVALID_THREADID) {
		thread_info* t;

		/* load thread state */
		if (threads[tid] == 0)
			threads[tid] = new_thread();

		t = threads[tid];
		exec_state_restore(&(t->state));
	}
}

void LSG_(run_thread)(ThreadId tid) {
    LSG_(switch_thread)(tid);
}

void LSG_(sync_current_thread)(void) {
	ThreadId tid = LSG_(current_tid);
	LSG_ASSERT(tid < VG_N_THREADS);

	if (tid != VG_INVALID_THREADID) {
		thread_info* t = threads[tid];
		LSG_ASSERT(t != 0);

		exec_state_save(&(t->state));
	}
}

/*------------------------------------------------------------*/
/*--- Execution states in a thread & signal handlers       ---*/
/*------------------------------------------------------------*/

static void exec_state_save(exec_state* state) {
	state->bound = LSG_(current_state).bound;
	state->records = LSG_(current_state).records;
}

static void exec_state_restore(exec_state* state) {
	LSG_(current_state).bound = state->bound;
	LSG_(current_state).records = state->records;
}
