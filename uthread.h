#include <ucontext.h>
#include <vector>
#include <stdio.h>
#include <queue>
#include <chrono>
typedef void (*Fun)(void *arg);

const int DEFAULT_STACK_SZIE = 1024*8;
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

typedef std::vector<uthread_t> Thread_vector;
 
typedef struct schedule_t
{
    ucontext_t main;
    int running_thread;
    Thread_vector threads;
    Thread_vector runnable_queue;
    Thread_vector suspend_queue;
    schedule_t():running_thread(-1){}

    
    void remove_runnable(int tid){
        if(runnable_queue.empty()){
            return;
        }
        auto end = runnable_queue.end();        
        for (Thread_vector::iterator i = runnable_queue.begin(); i != end; i++) {
            uthread_t t = *i;    
            if(t.tid == tid){
                runnable_queue.erase(i);
                printf("erase t:%d size:%d\n",tid,runnable_queue.size());
                break;
            }        
        }
    }
    void remove_suspend(int tid){
        if(suspend_queue.empty()){
            return;
        }
        auto end = suspend_queue.end();        
        for (Thread_vector::iterator i = suspend_queue.begin(); i != end; i++) {
            uthread_t t = *i;    
            if(t.tid == tid){
                printf("erase t:%d\n",tid);
                suspend_queue.erase(i);
                break;
            }        
        }
    }
}schedule_t;


int  uthread_create(schedule_t &schedule,Fun func,void *arg){
    int id = schedule.threads.size();
    schedule.threads.push_back(uthread_t{});
    uthread_t& thread = schedule.threads[id];
    printf("id:%d\n",id);
    thread.tid = id;
    thread.arg = arg;
    thread.func = func;    
    thread.state = RUNNABLE;    
    schedule.runnable_queue.push_back(thread);
    return id;
}
void uthread_yield(schedule_t &schedule)
{
    if(schedule.running_thread != -1 ){
        uthread_t *t = &(schedule.threads[schedule.running_thread]);
        t->state = SUSPEND;
        schedule.running_thread = -1;
        // schedule.runnable_queue.push_back(*t);
        swapcontext(&(t->ctx),&(schedule.main));
    }
}

void uthread_body(schedule_t* s){
    uthread_t& thread = s->threads[s->running_thread];
    thread.func(thread.arg);
    printf("co:%d exited\n",s->running_thread);
    s->threads.erase(s->threads.begin() + s->running_thread);
    s->running_thread = -1;
}
void uthread_resume(schedule_t &schedule , size_t id)
{
    if(id >= schedule.threads.size()){
        return;
    }
 
    uthread_t *t = &(schedule.threads[id]);
 
    switch(t->state){
        case RUNNABLE:
            getcontext(&(t->ctx));
 
            t->ctx.uc_stack.ss_sp = t->stack;
            t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
            t->ctx.uc_stack.ss_flags = 0;
            t->ctx.uc_link = &(schedule.main);
            t->state = RUNNING;
 
            schedule.running_thread = id;
 
            makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
            schedule.remove_runnable(id);
            swapcontext(&(schedule.main),&(t->ctx));
            break;
        case SUSPEND:            
            t->state = RUNNING;        
            schedule.running_thread = id;
            schedule.remove_suspend(id);
            schedule.remove_runnable(id);
            swapcontext(&(schedule.main),&(t->ctx));
            // printf("uthread_resume co:%d exited\n",id);
            break;
        default: ;
    }
}
int schedule_finished(schedule_t &schedule){
    return schedule.threads.size() == 0;
}
typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::milliseconds milliseconds;

struct timer{
    time_point timeout;
    int tid;
    timer(const timer& t){
        timeout = t.timeout;
        tid = t.tid;
    }
    timer(const time_point& timeout,int tid){
        this->timeout = timeout;
        this->tid = tid;
        // printf("timer:%d,tid:%d\n",timeout,tid);        
    }
    bool operator<(const timer& b) const { 
        return timeout > b.timeout; 
    } 
};


std::priority_queue<timer> sleepQueue;
void uthread_sleep(schedule_t &schedule,int timeoutms){
    time_point now = std::chrono::high_resolution_clock::now();
    int tid = schedule.running_thread;
    timer t(now,tid);
    t.timeout += milliseconds(timeoutms);

    sleepQueue.push(t);
    uthread_yield(schedule);
    schedule.remove_runnable(tid);
}

int uthread_check_timer(schedule_t &schedule){
    // printf("uthread_check_timer\n");
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
    uthread_resume(schedule,smallTime.tid);
    // printf("wake up:%d\n",smallTime.tid);
    return 0;
}

void uthread_check_runnable(schedule_t &schedule){
    if(schedule.runnable_queue.empty()){
        return;
    }
    uthread_t t = schedule.runnable_queue.front();
    printf("uthread_check_runnable t:%d\n",t.tid);
    uthread_resume(schedule,t.tid);
}