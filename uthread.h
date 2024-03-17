#include <ucontext.h>
#include <vector>
#include <stdio.h>
#include <queue>
#include <chrono>
#include <thread>
typedef void (*Fun)(void *arg);

const int DEFAULT_STACK_SZIE = 1024;
enum ThreadState{
    SUSPEND,
    RUNNABLE,
    RUNNING,
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

 
typedef struct schedule_t
{
    ucontext_t main;
    uthread_t* running_thread;
    std::deque<uthread_t*> runnable_queue;
    schedule_t():running_thread(0){}
}schedule_t;

void uthread_body(schedule_t* s){
    uthread_t* pthread = s->running_thread;
    pthread->func(pthread->arg);
    printf("co:%d exited\n",pthread->tid);
    s->running_thread = 0;
    delete pthread;
}


int  uthread_create(schedule_t &schedule,Fun func,void *arg){
    static int gtid = 0;
    int id = gtid++;
    uthread_t* pthread = new(uthread_t);
    uthread_t& thread = *pthread;
    thread.tid = id;
    thread.arg = arg;
    thread.func = func;    
    thread.state = RUNNABLE;    
    schedule.runnable_queue.push_back(pthread);
    printf("id:%d\n",id);
    return id;
}
void uthread_yield(schedule_t &schedule)
{
    if(schedule.running_thread){
        uthread_t *t = schedule.running_thread;
        t->state = SUSPEND;
        schedule.running_thread = 0;
        // schedule.runnable_queue.push_back(*t);
        swapcontext(&(t->ctx),&(schedule.main));
    }
}

void uthread_resume(schedule_t &schedule ,uthread_t* t)
{ 
    switch(t->state){
        case RUNNABLE:
            getcontext(&(t->ctx));
 
            t->ctx.uc_stack.ss_sp = t->stack;
            t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
            t->ctx.uc_stack.ss_flags = 0;
            t->ctx.uc_link = &(schedule.main);
            t->state = RUNNING;
 
            schedule.running_thread = t;
 
            makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
            swapcontext(&(schedule.main),&(t->ctx));
            break;
        case SUSPEND:            
            t->state = RUNNING;        
            schedule.running_thread = t;
            swapcontext(&(schedule.main),&(t->ctx));
            break;
        default: ;
    }
}
int schedule_finished(schedule_t &schedule){
    return 0;
}
typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::milliseconds milliseconds;

struct timer{
    time_point timeout;
    uthread_t* thread;
    timer(const timer& t){
        timeout = t.timeout;
        thread = t.thread;
    }
    timer(const time_point& timeout,uthread_t* thread){
        this->timeout = timeout;
        this->thread = thread;
        // printf("timer:%d,tid:%d\n",timeout,tid);        
    }
    bool operator<(const timer& b) const { 
        return timeout > b.timeout; 
    } 
};


std::priority_queue<timer> sleepQueue;
void uthread_sleep(schedule_t &schedule,int timeoutms){
    time_point now = std::chrono::high_resolution_clock::now();
    uthread_t* curthread = schedule.running_thread;
    timer t(now,curthread);
    t.timeout += milliseconds(timeoutms);

    sleepQueue.push(t);
    uthread_yield(schedule);
}

int uthread_check_timer(schedule_t &schedule){
    // printf("uthread_check_timer\n");
    while(true){
        if(sleepQueue.empty()){
            return 0;
        }
        time_point now = std::chrono::high_resolution_clock::now();
        timer smallTime = sleepQueue.top();
        auto diff = smallTime.timeout - now;
        int ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        if(ms > 0){  
            // printf("sleep %d diff:%d size:%d\n",smallTime.tid,std::chrono::duration_cast<std::chrono::milliseconds>(diff).count(),schedule.runnable_queue.size());      
            return ms;
        }
        // printf("will wakeup %d\n",smallTime.tid);
        sleepQueue.pop();
        now = std::chrono::high_resolution_clock::now();
        uthread_resume(schedule,smallTime.thread);
        time_point now2 = std::chrono::high_resolution_clock::now();
        // printf("switch:%d tid:%d\n",std::chrono::duration_cast<std::chrono::microseconds>(now2 - now).count(),smallTime.tid);
    }
    return 0;
}

void uthread_check_runnable(schedule_t &schedule){
    while(true){
        if(schedule.runnable_queue.empty()){
            return;
        }
        uthread_t* pthread = schedule.runnable_queue.front();
        schedule.runnable_queue.erase(schedule.runnable_queue.begin());
        time_point now1 = std::chrono::high_resolution_clock::now();
        uthread_resume(schedule,pthread);
        time_point now2 = std::chrono::high_resolution_clock::now();
        // printf("uthread_check_runnable t:%d %d\n",t.tid,std::chrono::duration_cast<std::chrono::microseconds>(now2 - now1).count());
    }
}

void uthread_loop(schedule_t& s){
    while(true){
        int waitms = uthread_check_timer(s);
        if(waitms > 0 && s.runnable_queue.empty()){
            std::this_thread::sleep_for(std::chrono::milliseconds(waitms));
        }
        uthread_check_runnable(s);
    }
}