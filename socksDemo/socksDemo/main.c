//
//  main.c
//  socksDemo
//
//  Created by penglei on 2020/4/22.
//  Copyright © 2020 penglei. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>

//c实现socks代理服务器

//创建一个监听套接字
int creat_listenfd(void)
{
    //创建TCP
    /*
     第一个参数：用来指定套接字使用的地址格式，通常使用AF_INET，是 IPv4 网络协议的套接字类型,
     常见的有AF_INET、AF_INET6、AF_LOCAL（或称AF_UNIX，Unix域socket）、AF_ROUTE等等
     第二个参数：指定套接字的类型，若是SOCK_DGRAM，则用的是udp不可靠传输
     第三个参数：配合type参数使用，指定使用的协议类型（当指定套接字类型后，可以设置为0，因为默认为UDP或TCP）
     */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int n = 1;
    //设置地址复用
    /*
     int setsockopt(int sockfd, int level, int optname,const void *optval, socklen_t optlen);
     sockfd：标识一个套接口的描述字。
     level：选项定义的层次；支持SOL_SOCKET、IPPROTO_TCP、IPPROTO_IP和IPPROTO_IPV6。
     optname：需设置的选项。
     optval：指针，指向存放选项待设置的新值的缓冲区。
     optlen：optval缓冲区长度。
     */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &n, 4);
    //要绑定给sockfd的协议地址 这里是创建ipv4的地址
    struct sockaddr_in sin;
    //将结构体空间清零
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    //绑定端口号范围：1024 ~ 49151
    sin.sin_port = htons(2020);
/*绑定IP地址只需绑定INADDR_ANY，管理一个套接字就行，不管数据是从哪个网卡过来的，只要是绑定的端口号过来的数据，都可以接收到。INADDR_ANY转换过来就是0.0.0.0，泛指本机的意思，也就是表示本机的所有IP
     */
    sin.sin_addr.s_addr = INADDR_ANY;
    
    //绑定套接字到一个本地地址
    //返回值：成功返回0，失败返回-1.
    if(bind(fd, (struct sockaddr *)&sin, sizeof(sin))<0)
    {
        perror("bind");
        exit(-1);
    }
    /*
     创建一个套接口并监听申请的连接.
     int listen( int sockfd, int backlog);
     sockfd：用于标识一个已捆绑未连接套接口的描述字。
     backlog：等待连接队列的最大长度。
     */
    if(listen(fd, 100)<0) {
        perror("listen");
        exit(-1);
    }
    return fd;
}

void forward(int fd1,int fd2)
{
    //先侦查那个fb能用 能用就去读，然后发给另一个
    char buffer[10000] = {0};
    fd_set fdset;
    while (1) {
        FD_ZERO(&fdset);
        FD_SET(fd1,&fdset);
        FD_SET(fd2,&fdset);
        int maxfd = (fd1>fd2?fd1:fd2)+1;
        int r = select(maxfd,&fdset,NULL,NULL,0);
        if(r<=0) break;
        //表示fd1可读
        if(FD_ISSET(fd1,&fdset))
        {
            int rlen = read(fd1, buffer, sizeof(buffer));
            if(rlen<=0) break;
            int wlen = write(fd2, buffer, rlen);
            if(wlen<=0) break;
                
            
        }
        if(FD_ISSET(fd2,&fdset))
        {
            int rlen = read(fd2, buffer, sizeof(buffer));
            if(rlen<=0) break;
            int wlen = write(fd1, buffer, rlen);
             if(wlen<=0) break;
               
        }
    }
    
}

//处理和客户端的通信
void *hander(void *arg)
{
    int clientfd = (int)arg;
    printf("传进来的fd是%d\n",(int)arg);
   
    //创建一个缓冲区
    char buffer[10240] = {0};
    //这里根据sock协议服务器进行响应客户端
    int rlen = read(clientfd,buffer,sizeof(buffer));
    if (rlen <=0)  return NULL;
    if(buffer[0]!=5) return NULL;
    //认证ack给客户端
    buffer[0]=5;
    buffer[1]=0;
    int wlen = write(clientfd,buffer,2);
   
    //接受客户端发来的连接请求
    
    rlen = read(clientfd, buffer, sizeof(buffer));
    if(rlen<=0) return NULL;
    if(buffer[0]!=5 || buffer[1]!=1) return NULL;
    //先连接目标服务器
    struct sockaddr_in sin;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr,buffer+4,4);//将客户端传过来目标服务器的ip拷贝过来
    memcpy(&sin.sin_port,buffer+8,2);//将客户端传过来目标服务器的端口号拷贝过来
    //c创建连接目标服务器的socket
    int dstfd = socket(AF_INET, SOCK_STREAM, 0);
    if(connect(dstfd, (struct sockaddr *)&sin, sizeof(sin))<0) {
        perror("connect");
        return NULL;
    }
    printf("连接成功\n");
    //关闭文件描述符 让文件描述符不被耗光 为了防止文件描述符一直递增，文件描述符最大为1024；
    //服务器连接成功目标服务器需要给客户端回应
    //回应连接请求
    bzero(buffer, sizeof(buffer));
    buffer[0]=5;
    buffer[1]=0;
    buffer[2]=0;
    buffer[3]=1;
    wlen = write(clientfd, buffer, 10);
    //发送失败
    if(wlen!=10) return NULL;
        
   //完成连接后开始转发数据
        
    forward(clientfd,dstfd);
    close(dstfd);
    close(clientfd);
    return NULL;
}

int main(int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    
    //创建一个监听套接字
    int listenfd = creat_listenfd();
    //循环接受客户的连接请求
    while (1)
    {
        //从队列里面获取连接了的客户，去通信
        /*
         SOCKET accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
         参数
         sockfd：套接字描述符，该套接口在listen()后监听连接。
      addr：（可选）指针，指向一缓冲区，其中接收为通讯层所知的连接实体的地址。Addr参数的实际格式由套接口创建时所产生的地址族确定。
         addrlen：（可选）指针，输入参数，配合addr一起使用，指向存有addr地址长度的整型数。
         */
        int clientfd = accept(listenfd, NULL, NULL);
        //开启一个新的线程去处理，继续监听
        pthread_t tid;
        pthread_create(&tid,NULL,hander,(void *)clientfd);
        
    }
    return 0;
}
