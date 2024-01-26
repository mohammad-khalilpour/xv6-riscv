#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define FREE 0x0
#define RUNNING 0x1
#define RUNNABLE 0x2
#define STACK_SIZE 8192
#define MAX_THREAD 4

struct context {
    uint64 ra;
    uint64 sp;

    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct uThread{
    char stack[STACK_SIZE];
    struct context context;
    int state;
};

extern void uThreadSwitch(struct context*, struct context*);

struct uThread uThreads[MAX_THREAD];

struct uThread *currentThreadPointer;

void uThreadSchedule() {
    struct uThread *nextThreadPointer, *t;
    t = 0;

    nextThreadPointer = currentThreadPointer + 1;
    for(int i=0;i< MAX_THREAD;i++) {
        if (nextThreadPointer >= uThreads + MAX_THREAD)
            nextThreadPointer = uThreads;
        if(nextThreadPointer->state == RUNNABLE) {
            t = nextThreadPointer;
            break;
        }

        nextThreadPointer = nextThreadPointer + 1;
    }


    if (t == 0) {
        printf("There isn't any runnable threads\n");
        exit (-1);
    }

    if (t != currentThreadPointer) {
        t->state = RUNNING;
        nextThreadPointer = currentThreadPointer;
        currentThreadPointer = t;
        uThreadSwitch(&nextThreadPointer->context, &currentThreadPointer->context);


        // void (*functionPtr)(void) = (void (*)(void))nextThreadPointer->context.ra;
        // functionPtr();
    } else {
        t = 0;
    }

}

void
threadYield(void)
{
    currentThreadPointer->state = RUNNABLE;
    uThreadSchedule();
}

void
thread_create(void (*func)())
{
    struct uThread *thread;

    for (thread = uThreads; thread < uThreads + MAX_THREAD; thread++) {
        if (thread->state == FREE) break;
    }
    thread->state = RUNNABLE;
    memset(&thread->context, 0, sizeof thread->context);
    thread->context.ra = (uint64)func;

    thread->context.sp = (uint64)(thread->stack + STACK_SIZE);
}

volatile int out_a, out_b, out_c;

void
thread_a(void)
{
    int start_time = uptime();

    while(out_a < 100000000) {
        if (uptime() - start_time < 10) {
            out_a += 1;
        }
        else {
            printf("thread_a:%d\n", out_a);
            threadYield();
            start_time = uptime();
        }
    }

    currentThreadPointer->state = FREE;
    uThreadSchedule();
}

void
thread_b(void)
{
    int start_time = uptime();

    while(out_b < 100000000) {
        if (uptime() - start_time < 10){
            out_b += 1;
        }
        else {
            printf("thread_b:%d\n", out_b);
            threadYield();
            start_time = uptime();
        }
    }

    currentThreadPointer->state = FREE;
    uThreadSchedule();
}

void
thread_c(void)
{
    int start_time = uptime();

    while(out_c < 100000000) {
        if (uptime() - start_time < 10){
            out_c += 1;
        }
        else {
            printf("thread_c:%d\n", out_c);
            threadYield();
            start_time = uptime();
        }
    }

    currentThreadPointer->state = FREE;
    uThreadSchedule();
}


int
main()
{
    out_a = 1;
    out_b = 1;
    out_c = 1;
    currentThreadPointer = &uThreads[0];
    currentThreadPointer->state = RUNNING;
    thread_create(thread_a);
    thread_create(thread_b);
    thread_create(thread_c);
    uThreadSchedule();

    return 0;
}
