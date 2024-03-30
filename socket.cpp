#include "uthread.h"
#include <netinet/tcp.h>
#include <sys/epoll.h>

static schedule_t* schedule = 0;
void socket_set_schedule(schedule_t* s){
    schedule = s;
    schedule->efd = epoll_create(1024);
}
struct socket_data{
    int fd;
    uthread_t* thread;
};

class Time{
public:
    time_point start;
    Time(){
        start =  std::chrono::high_resolution_clock::now();
    }
    ~Time(){
        time_point stop =  std::chrono::high_resolution_clock::now();
        printf("use :%d\n",std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    }
};
int u_epoll_inner(epoll_event& evt){
    // Time t;
    // epoll_event evt;
    // evt.events = POLLOUT | POLLIN | POLLERR | POLLHUP;
    socket_data data;
    data.fd = evt.data.fd;
    data.thread = schedule->running_thread;
    evt.data.ptr = &data;
    
    // printf("u_epoll_inner fd:%d\n",data.fd);
    int ret = epoll_ctl(schedule->efd, EPOLL_CTL_ADD,data.fd, &evt);
    // printf("[[u_epoll_inner yield tid:%d\n",data.thread->tid);
    uthread_yield(*schedule);
    ret = epoll_ctl(schedule->efd,EPOLL_CTL_DEL,data.fd,0);
    // printf("]]u_epoll_inner yield tid:%d\n",data.thread->tid);
    return ret;
}
int u_socket(int domain, int type, int protocol){
    int fd = socket(domain, type, protocol);
    if (fd == -1) {
        printf("Failed to create a new socket\n");
        return -1;
    }
    int ret = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (ret == -1) {
        close(ret);
        return -1;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, (char *) &reuse, sizeof(reuse));
    return fd;
}

int u_accept(int fd, struct sockaddr *addr, socklen_t *len){
    // printf("u_accept fd:%d\n",fd);
    int sockfd;
    while (1) {
        epoll_event evt;
        evt.events = EPOLLIN | EPOLLERR | EPOLLHUP ;
        evt.data.fd = fd;
        u_epoll_inner(evt);
        // printf("accept new client\n");
        //resume
        sockfd = accept(fd, addr, len);
        if (sockfd < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            else if (errno == ECONNABORTED) {
                printf("accept : ECONNABORTED\n");
            }
            else if (errno == EMFILE || errno == ENFILE) {
                printf("accept : EMFILE || ENFILE\n");
            }
            return -1;
        }
        else {
            break;
        }
    }
    int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
    if (ret == -1) {
        close(sockfd);
        return -1;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse));
    return sockfd;
}

ssize_t u_recv(int fd, void *buf, size_t len, int flags){
    epoll_event evt;
    evt.events = EPOLLIN | EPOLLERR | EPOLLHUP ;
    evt.data.fd = fd;
    u_epoll_inner(evt);
    //resume
    // printf("[[will recv tid:%d\n",schedule->running_thread->tid);
    ssize_t ret = recv(fd, buf, len, flags);
    // printf("]]will recv tid:%d ret:%d\n",schedule->running_thread->tid,ret);
    return ret;

}

ssize_t u_send(int fd, const void *buf, size_t len, int flags){
    int sent = 0;
    int ret = send(fd, ((char *) buf) + sent, len - sent, flags);
    if (ret <= 0) return ret;
    if (ret > 0) sent += ret;
    while (sent < len) {
        epoll_event evt;
        evt.events = EPOLLOUT | EPOLLERR | EPOLLHUP ;
        evt.data.fd = fd;
        u_epoll_inner(evt);
        // printf("[[will send tid:%d\n",schedule->running_thread->tid);
        ret = send(fd, ((char *) buf) + sent, len - sent, flags);
        // printf("]]will send tid:%d ret:%d\n",schedule->running_thread->tid,ret);
        //printf("send --> len : %d\n", ret);
        if (ret <= 0) {
            break;
        }
        sent += ret;
    }
    if (ret <= 0 && sent == 0) return ret;
    return sent;
}

int u_close(int fd){
    return close(fd);
}

int u_connect(int fd, struct sockaddr *name, socklen_t len){
    int ret = 0;
    while (1) {
        //加入epoll，然后yield
        epoll_event evt;
        evt.events = EPOLLOUT| EPOLLERR | EPOLLHUP ;
        evt.data.fd = fd;
        u_epoll_inner(evt);
        //resume
        ret = connect(fd, name, len);
        if (ret == 0) break;
        if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)) {
            continue;
        }
        else {
            break;
        }
    }
    return ret;
}


void socket_check(){
    epoll_event eventlist[1];
    int waitms = 10;
    if(schedule->sleep_queue.size() > 0 || schedule->runnable_queue.size() > 0 ){
        waitms = 0;
    }
    int evts = epoll_wait(schedule->efd,eventlist,sizeof(eventlist)/sizeof(epoll_event),waitms);
    // printf("epoll wait evts:%d\n",evts);
    for(int i = 0;i<evts;i++){
        epoll_event* evt = &eventlist[i];
        socket_data* data = (socket_data*)evt->data.ptr;
        // printf("wait fd:%d\n",data->fd);
        uthread_resume(*schedule,data->thread);
    }
}