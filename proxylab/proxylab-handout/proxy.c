#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 4
#define SBUFSIZE 16
//void echo_cnt(int connfd);
void *thread(void *vargp);

sbuf_t sbuf;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void parse_url(char *url, char *hostname, char *port, char *path);
void build_header(char *server,  char *hostname, char *port, char *path, rio_t *rio);

int main(int argc, char *argv[])
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    pthread_t tid;

    /*check command-line args*/
    if(argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[1]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);

    //-------
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i = 0; i < NTHREADS; i++)
    {
        Pthread_create(&tid, NULL, thread, NULL);
    }


    //-------

    while(1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        //向信号量缓冲区写入这个文件描述符
        sbuf_insert(&sbuf, connfd);

        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s)\n", hostname, port);

        //doit(connfd);
        //Close(connfd);
    }
    return 0;
}

void doit(int fd)
{
    //int is_static;
    //struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    //char filename[MAXLINE], cgiargs[MAXLINE];
    int serverfd, n;
    rio_t rio, se_rio;

    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    char server[MAXLINE];

    //面对客户端,自己是服务器,接受请求
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);

    if(strcasecmp(method, "GET"))
    {
        printf("Proxy does not implement the method");
        return;
    }

    //解析url
    parse_url(url, hostname, port, path);

    //构造新的请求头部
    build_header(server, hostname, port, path, &rio);


    serverfd = Open_clientfd(hostname, port);
    if(serverfd < 0)
    {
        fprintf(stderr, "connect to real server err");
        return;
    }

    Rio_readinitb(&se_rio, serverfd);
    Rio_writen(serverfd, server, strlen(server));

    while((n = Rio_readlineb(&se_rio, buf, MAXLINE)) != 0)
    {
        printf("get %d bytes from server\n", n);
        Rio_writen(fd, buf, n);
    }
    Close(serverfd);

}

void parse_url(char *url, char *hostname, char *port, char *path)
{
    //http://localhost: 12345/home.html
    char *hostpos = strstr(url, "//");
    if(hostpos == NULL)
    {
        char *pathpos = strstr(url, "/");
        if(pathpos != NULL)
        {
            strcpy(path, pathpos);
            strcpy(port, "80");
            return;
        }

        hostpos = url;
    }
    else
    {
        hostpos += 2;
    }

    char *portpose = strstr(hostpos, ":");
    if(portpose != NULL)
    {
        int tmp;
        char *pathpose = strstr(portpose, "/");
        if(pathpose != NULL)
            sscanf(portpose + 1, "%d%s", &tmp, path);
        else
            sscanf(portpose + 1, "%d", &tmp);

        sprintf(port, "%d", tmp);
        *portpose = '\0';
    }
    else
    {
        char *pathpose = strstr(hostpos, "/");
        if(pathpose != NULL)
        {
            strcpy(path, pathpose);
            *pathpose = '\0';
        }
        strcpy(port, "80");
    }
    strcpy(hostname, hostpos);
    return;
}

void build_header(char *server,  char *hostname, char *port, char *path, rio_t *rio)
{
    char *User_Agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
    char *conn_hdr = "Connection: close\r\n";
    char *prox_hdr = "Proxy-Connection: close\r\n";
    char *host_hdr_format = "Host: %s\r\n";
    char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
    char *endof_hdr = "\r\n";

    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    sprintf(request_hdr, requestlint_hdr_format, path);
    while(Rio_readlineb(rio, buf, MAXLINE) > 0)
    {
        if(strcmp(buf, endof_hdr) == 0)
            break;

        if(!strncasecmp(buf, "Host", strlen("Host")))
        {
            strcpy(host_hdr, buf);
            continue;
        }

        if(!strncasecmp(buf, "Connection", strlen("Connection"))
            && !strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")))
        {
            strcat(other_hdr, buf);
        }
    }
    if(strlen(host_hdr) == 0)
    {
        sprintf(host_hdr, host_hdr_format, hostname);
    }
    sprintf(server, "%s%s%s%s%s%s%s",
        request_hdr,
        host_hdr,
        conn_hdr,
        prox_hdr,
        User_Agent,
        other_hdr,
        endof_hdr);
    return;
}

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());
    while (1)
    {
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
}