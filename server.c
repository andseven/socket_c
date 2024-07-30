#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "threadpool.h"

void working(void* arg);
void acceptConn(void* arg);

typedef struct SockInfo {
    struct sockaddr_in saddr;
    int fd;
}SockInfo;

typedef struct PoolInfo {
    ThreadPool* pool;
    int fd;
}PoolInfo;

int main() {
    //创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }
    printf("socket build successed...\n");

    //绑定客户端IP和PORT
    struct sockaddr_in saddr;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    int ret = bind(fd,(struct sockaddr*)&saddr, sizeof(saddr));
    if(ret == -1) {
        perror("bind");
        return  -1;
    }
    printf("bind successed...\n");

    //设置监听
    ret = listen(fd,128);
    if(ret == -1) {
        perror("listen");
        return  -1;
    }
    printf("listening...\n");

    //创建线程池
    ThreadPool* pool = threadPoolcreate(3,8,100);
    PoolInfo* info = (struct PoolInfo*)malloc(sizeof(PoolInfo));
    info->pool = pool;
    info->fd = fd;
    threadPoolAdd(pool,acceptConn,info);
    pthread_exit(NULL);
    return 0;
}

void working(void* arg) {
    SockInfo* pinfo = (SockInfo*)arg;
    char ip[32];
    printf("client's IP:%s,PORT:%d\n",
        inet_ntop(AF_INET,&pinfo->saddr.sin_addr.s_addr,ip,sizeof(ip)),
        ntohs(pinfo->saddr.sin_port));
    //通信
    while (1) {
        char buffer[1024];
        int len = recv(pinfo->fd,buffer,sizeof(buffer),0);
        if(len>0) {
            printf("client say:%s\n",buffer);
            send(pinfo->fd,buffer,len,0);
        }
        else if(len == 0 ) {
            printf("client disconnected...\n");
            break;
        }
        else {
            perror("recv");
            break;
        }
    }
    close(pinfo->fd);
}

void acceptConn(void *arg) {
    //等待客户端连接
    PoolInfo* pool_info =(PoolInfo*)arg;
    int addlen = sizeof(struct sockaddr_in);
    while (1) {
        SockInfo* pinfo;
        pinfo = (struct SockInfo*)malloc(sizeof(SockInfo));
        pinfo->fd = accept(pool_info->fd,(struct  sockaddr*)&pinfo->saddr,&addlen);
        if(pinfo->fd == -1) {
            perror("accept");
            break;
        }
        //添加通信任务
        threadPoolAdd(pool_info->pool,working,pinfo);
    }
    close(pool_info->fd);
}