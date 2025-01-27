#include <ucontext.h>
#include <vector>
#include <stdio.h>
#include <queue>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <fcntl.h>

#ifndef __UTHREAD_H__
#define __UTHREAD_H__

typedef void (*Fun)(void *arg);

const int DEFAULT_STACK_SZIE = 1024*4;
enum ThreadState{
    SUSPEND,
    RUNNABLE,
    RUNNING,
    STOP,
};
typedef struct uthread_t
{
    int tid;
    ucontext_t ctx;
    Fun func;
    void *arg;
    enum ThreadState state;
    char stack[DEFAULT_STACK_SZIE];
}uthread_t;

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::milliseconds milliseconds;


struct timer{
    time_point timeout;
    uthread_t* thread;
    timer(const timer& t);
    timer(const time_point& timeout,uthread_t* thread);
    bool operator<(const timer& b) const ;
};
 
typedef struct schedule_t
{
    ucontext_t main;
    uthread_t* running_thread;
    std::priority_queue<timer> sleep_queue;
    std::deque<uthread_t*> runnable_queue;
    int efd;
    schedule_t();
}schedule_t;



int  uthread_create(schedule_t &schedule,Fun func,void *arg);
void uthread_yield(schedule_t &schedule);
void uthread_ready(schedule_t &schedule);
void uthread_resume(schedule_t &schedule ,uthread_t* t);
int schedule_finished(schedule_t &schedule);



void uthread_sleep(schedule_t &schedule,int timeoutms);

int uthread_check_timer(schedule_t &schedule);
void uthread_check_runnable(schedule_t &schedule);

void uthread_loop(schedule_t& s);


int u_socket(int domain, int type, int protocol);

int u_accept(int fd, struct sockaddr *addr, socklen_t *len);

ssize_t u_recv(int fd, void *buf, size_t len, int flags);

ssize_t u_send(int fd, const void *buf, size_t len, int flags);

int u_close(int fd);

int u_connect(int fd, struct sockaddr *name, socklen_t len);


void socket_set_schedule(schedule_t* s);
void socket_check();

#endif //__UTHREAD_H__