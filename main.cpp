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
    // for(int i = 0;true;i++){       
        uthread_sleep(*(schedule_t *)arg,300);
        // printf("i:%d\n",i);
    // }
}
 
void func3(void *arg)
{
    puts("3333");
    puts("3333");
    uthread_sleep(*(schedule_t *)arg,1000);
    puts("3333");
    puts("3333");
    for(int i = 0;true;i++){       
        uthread_sleep(*(schedule_t *)arg,500);
        printf("i:%d\n",i);
    }
 
}

void testsleep(void* arg){
    schedule_t& s = *(schedule_t*)arg;
    
    for(int i = 0;true;i++){       
        time_point now1 = std::chrono::high_resolution_clock::now();
        uthread_sleep(s,10);
        time_point now2 = std::chrono::high_resolution_clock::now();
        switch(s.running_thread->tid){
            case 0:
            case 99:
            case 999:
            case 9999:
            int count = std::chrono::duration_cast<std::chrono::microseconds>(now2 - now1).count();
            printf("d:%d tid:%d ssize:%d\n",count,s.running_thread,sleepQueue.size());
            break;
        }        
    }
}
 
void schedule_test()
{
    schedule_t s;
 
    int id1 = uthread_create(s,func3,&s);
    int id2 = uthread_create(s,func2,&s);
    for(int i = 0;i<1000;i++){       
        // uthread_sleep(*(schedule_t *)arg,500);
        // printf("i:%d\n",i);
        uthread_create(s,testsleep,&s);
    } 
    uthread_loop(s);
    puts("main over");
 
}
int main()
{

    schedule_test();
 
    return 0;
}
