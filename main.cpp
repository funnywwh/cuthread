#include "uthread.h"
#include <stdio.h>
#include <thread>
 
void func2(void * arg)
{
    puts("22");
    puts("22");
    uthread_sleep(*(schedule_t *)arg,3000);
    puts("222");
    puts("222");
}
 
void func3(void *arg)
{
    puts("3333");
    puts("3333");
    uthread_sleep(*(schedule_t *)arg,1000);
    puts("3333");
    puts("3333");
    for(int i = 0;true;i++){       
        uthread_sleep(*(schedule_t *)arg,2000);
        printf("i:%d\n",i);
    }
 
}
 
void schedule_test()
{
    schedule_t s;
 
    int id1 = uthread_create(s,func3,&s);
    int id2 = uthread_create(s,func2,&s);
 
    while(!schedule_finished(s)){
        int waitms = uthread_check_timer(s);
        if(waitms > 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(waitms));
        }
        // uthread_resume(s,id1);
        // uthread_resume(s,id2);
        uthread_check_runnable(s);
    }
    puts("main over");
 
}
int main()
{
    schedule_test();
 
    return 0;
}
