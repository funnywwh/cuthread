#include "uthread.h"
#include <stdio.h>
#include <thread>
#include <arpa/inet.h>
#include <string.h>

#include <sys/epoll.h>
#include <sys/poll.h>


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
    char buf[2048];
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
            // printf("d:%d tid:%d ssize:%d\n",count,s.running_thread,s.sleep_queue.size());
            break;
        }        
    }
}
 
struct client{
    schedule_t* s;
    int fd;
};
void echo_loop(void* arg){
    client* pclient = (client*)arg;
    client c = *pclient;
    delete(pclient);

    printf("client_loop\n");
    while(true){
        char buf[1024];
        int ret = u_recv(c.fd,buf,1024,0);
        if(ret <= 0 ){
            printf("recv ret:%d\n",ret);
            break;
        }
        ret = u_send(c.fd,buf,ret,0);
        if(ret <= 0 ){
            printf("send ret:%d\n",ret);
            break;
        }
    }
    u_close(c.fd);
}

void http_loop(void* arg){
    client* pclient = (client*)arg;
    client c = *pclient;
    delete(pclient);
    while(true){
        
        char buf[100];
        while(true){
            int ret = u_recv(c.fd,buf,99,0);
            if(ret <= 0 ){
                return ;
            }
            buf[ret] = 0;
            // printf("recv len:%d data:%s\n",ret,buf);
            if(buf[ret-1] == '\n'){
                // printf("recv body\n");
                break;
            }
        }
        // delete buf;
        // const char* contextType = "context-type: text/plain\r\n";
        // const char* conntionClose = "connection: close\r\n";
        // // printf("sending:%s\n",contextType);
        // int ret = u_send(c.fd,contextType,strlen(contextType),0);
        // if(ret <= 0){
        //     printf("send ret:%d\n",ret);
        //     break;
        // }
        // // printf("sending:%s\n",conntionClose);
        // ret = u_send(c.fd,conntionClose,strlen(conntionClose),0);
        // if(ret <= 0){
        //     printf("send ret:%d\n",ret);
        //     break;
        // }
        // const char* contentLength = "content-length:12\r\n";
        // // printf("sending:%s\n",contentLength);
        // ret = u_send(c.fd,contentLength,strlen(contentLength),0);
        // if(ret <= 0){
        //     printf("send ret:%d\n",ret);
        //     break;
        // }
        // const char* bodyBegin = "\r\n";
        // // printf("sending:%s\n","body begin");
        // ret = u_send(c.fd,bodyBegin,strlen(bodyBegin),0);
        // if(ret <= 0){
        //     printf("send ret:%d\n",ret);
        //     break;
        // }
        // printf("sending:%s\n","body");
        const char* body = "context-type: text/plain\r\nconnection: close\r\ncontent-length:21\r\n\r\nmessage from server!\n";
        int ret = u_send(c.fd,body,strlen(body),0);
        if(ret <= 0){
            printf("send ret:%d\n",ret);
            break;
        }
        // u_close(c.fd);
        break;
    }
    // printf("http loop exit tid:%d\n",c.s->running_thread->tid);
    u_close(c.fd);
}
void socket_server(void* arg){
    schedule_t* s = (schedule_t*)arg;
    int sfd = u_socket(AF_INET, SOCK_STREAM, 0);
    if(sfd <= 0){
        printf("socket_server u_socket ret:%d\n",sfd);
        return ;
    }
    epoll_event evt;
    evt.data.fd = sfd;
    struct sockaddr_in local, remote;
    local.sin_family = AF_INET;
    local.sin_port = htons(8000);
    local.sin_addr.s_addr = INADDR_ANY;
    int ret = bind(sfd, (struct sockaddr *) &local, sizeof(struct sockaddr_in));
    if(ret < 0){
        printf("socket_server bind ret:%d\n",ret);
        return ;
    }
    ret = listen(sfd, 10);
    if(ret < 0){
        printf("socket_server listen ret:%d\n",ret);
        return ;
    }
    while(true){
        socklen_t len = sizeof(struct sockaddr_in);
        int cfd = u_accept(sfd,(struct sockaddr *)&remote,&len);
        if(cfd > 0 ){
            // printf("accept client:%d\n",cfd);
            client* c = new(client);
            c->s = s;
            c->fd = cfd;
            // uthread_create(*s,echo_loop,c);
            uthread_create(*s,http_loop,c);
            // http_loop(c);
        }else{
            printf("u_accept ret:%d\n",cfd);
            return;
        }
    }
}
void schedule_test()
{
    schedule_t s;
    // int id1 = uthread_create(s,func3,&s);
    // int id2 = uthread_create(s,func2,&s);
    // for(int i = 0;i<1000;i++){       
    //     // uthread_sleep(*(schedule_t *)arg,500);
    //     // printf("i:%d\n",i);
    //     uthread_create(s,testsleep,&s);
    // }
    // socket_set_schedule(&s);
    uthread_create(s,socket_server,&s);
    // socket_server(&s);
    uthread_loop(s);
    puts("main over");
 
}
int main()
{

    schedule_test();
 
    return 0;
}
