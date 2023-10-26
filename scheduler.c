/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

#undef _FORTIFY_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "system.h"
#include "scheduler.h"

#define STACK_SIZE 1024 * 1024

static struct {
    struct thread *head;
    struct thread *currentThread;
    jmp_buf ctx;
} state;

struct thread {
    jmp_buf ctx;
    enum {
        STATUS_,
        STATUS_RUNNING,
        STATUS_SLEEPING,
        STATUS_TERMINATING
    } status;
    struct {
        void *memory_; /* (to be allocated and freed) */
        void *memory; /* (points to the actual page) */
    } stack;
    struct thread *link;
    scheduler_fnc_t fnc;
    void *arg;
};

void initialize_state() {
    state.head = NULL;
    state.currentThread = NULL;

    if (setjmp(state.ctx) == 0) {}
}

int scheduler_create(scheduler_fnc_t fnc, void *arg) {

    size_t ps = page_size();
    
    /* Initialise thread structure */
    struct thread *t = (struct thread *) malloc(sizeof(struct thread));

    /* printf("page size: %ld", ps); */
    t -> status = STATUS_;

    if ((t -> stack.memory_ = malloc(STACK_SIZE + ps)) == NULL) {
        TRACE("scheduler create malloc");
        return -1;
    }
    t -> stack.memory = memory_align(t -> stack.memory_, ps);
    
    t -> link = state.head;
    state.head = t;

    t -> fnc = fnc;
    t -> arg = arg;

    return 0;
}

void scheduler_execute(void) {
    setjmp(state.ctx);
    schedule();
    destroy();
}

void scheduler_yield(void) {
    if (setjmp(state.currentThread->ctx) == 0) {
        state.currentThread -> status = STATUS_SLEEPING;
        longjmp(state.ctx, 1);
    }
    return;
}

/* Signal handler for SIGALRM*/
void scheduler_timer_handler(int i) 
{
    assert(SIGALRM == i);

    if(signal(SIGALRM,scheduler_timer_handler) == SIG_ERR)
    {
        TRACE("alarm");
        exit(-1);
    }

    alarm(1);
    
    scheduler_yield();
}

void schedule(void) {
    struct thread* candidate;

    candidate = thread_candidate();

    if (candidate == NULL) 
    {
        return;
    }
    else {
        state.currentThread = candidate;

        if (state.currentThread->status == STATUS_) {
            scheduler_fnc_t fnc;
            uint64_t rsp;
            fnc = candidate->fnc;

            alarm(1);

            rsp = (long)candidate->stack.memory + STACK_SIZE;
            __asm__ volatile ("mov %[rs], %%rsp \n" : [rs] "+r" (rsp) ::);

            fnc(candidate->arg);

            state.currentThread->status = STATUS_TERMINATING;
           
            longjmp(state.ctx, 1);
        }
        else {
            candidate->status = STATUS_RUNNING;
            longjmp(candidate->ctx, 1);
        }
    }
}

struct thread *thread_candidate(void) {
    struct thread *next;
    struct thread *candidate;
    
    if (state.head == NULL) {
        return NULL;
    }

    if (state.currentThread == NULL) /* case when current node is at last of linked list */
    {
        state.currentThread = state.head;
        return state.currentThread;
    }

    next = state.currentThread -> link; /* current = course, next = this */
    if (next == NULL) {
        next = state.head;
    }
    
    candidate = NULL;

    do 
    {
        if (next->status == STATUS_ || next->status == STATUS_SLEEPING) {
            candidate = next;
            break;
        }

        next = next->link;

        if (next == NULL) {
            next = state.head;
            continue;
        }
    } while(next != state.currentThread -> link);

    if (candidate == NULL) {
        return NULL;
    } else {
        return candidate;
    }
}

void destroy(void) {
    struct thread* t = state.head;

    while (t != NULL) {
        struct thread* next_thread = t->link;

        free(t->stack.memory_);
        free(t);

        t = next_thread;
    }

    state.head = NULL;
    state.currentThread = NULL;

    return;
}