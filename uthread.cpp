#include "uthread.h"

schedule_t::schedule_t():
    running_thread(0)
    ,efd(0)
{

}

// std::priority_queue<timer> schedule_t::sleep_queue;

void uthread_body(schedule_t* s){
    uthread_t* pthread = s->running_thread;
    // printf("[[co:%d\n",pthread->tid);
    pthread->func(pthread->arg);
    s->running_thread = 0;
    // printf("]]co:%d\n",pthread->tid);
    pthread->state = STOP;
    // delete pthread;
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
    // printf("id:%d\n",id);
    return id;
}

void uthread_ready(schedule_t &schedule){
    if(schedule.running_thread){
        schedule.runnable_queue.push_back(schedule.running_thread);
    }
}
void uthread_yield(schedule_t &schedule)
{
    if(schedule.running_thread){
        uthread_t *t = schedule.running_thread;
        t->state = SUSPEND;
        schedule.running_thread = 0;
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
            makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
 

            t->state = RUNNING;
            schedule.running_thread = t;
            // printf("running tid:%d\n",t->tid);
            swapcontext(&(schedule.main),&(t->ctx));
            break;
        case SUSPEND:            
            t->state = RUNNING;        
            schedule.running_thread = t;
            // printf("[[running tid:%d\n",t->tid);
            swapcontext(&(schedule.main),&(t->ctx));
            // printf("]]running tid:%d\n",t->tid);
            break;
        default: ;
    }
    if(t->state == STOP){
        // printf("delete tid:%d\n",t->tid);
        delete t;
    }

}
int schedule_finished(schedule_t &schedule){
    return 0;
}
timer::timer(const timer& t){
    timeout = t.timeout;
    thread = t.thread;
}
timer::timer(const time_point& timeout,uthread_t* thread){
    this->timeout = timeout;
    this->thread = thread;
    // printf("timer:%d,tid:%d\n",timeout,tid);        
}
bool timer::operator<(const timer& b) const { 
    return timeout > b.timeout; 
}

void uthread_sleep(schedule_t &schedule,int timeoutms){
    time_point now = std::chrono::high_resolution_clock::now();
    uthread_t* curthread = schedule.running_thread;
    timer t(now,curthread);
    t.timeout += milliseconds(timeoutms);

    schedule.sleep_queue.push(t);
    uthread_yield(schedule);
}

int uthread_check_timer(schedule_t &schedule){
    // printf("uthread_check_timer\n");
    while(true){
        if(schedule.sleep_queue.empty()){
            return 0;
        }
        time_point now = std::chrono::high_resolution_clock::now();
        timer smallTime = schedule.sleep_queue.top();
        auto diff = smallTime.timeout - now;
        int ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        if(ms > 0){  
            // printf("sleep %d diff:%d size:%d\n",smallTime.tid,std::chrono::duration_cast<std::chrono::milliseconds>(diff).count(),schedule.runnable_queue.size());      
            return ms;
        }
        // printf("will wakeup %d\n",smallTime.tid);
        schedule.sleep_queue.pop();
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
    socket_set_schedule(&s);
    while(true){
        int waitms = uthread_check_timer(s);
        if(waitms > 0 && s.runnable_queue.empty()){
            std::this_thread::sleep_for(std::chrono::milliseconds(waitms));
        }
        socket_check();
        uthread_check_runnable(s);
        printf("loop checked\n");
    }
}