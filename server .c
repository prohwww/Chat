//Serv.c
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<signal.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/file.h>
#include<unistd.h>
#include<errno.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<netdb.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<bits/select.h>
#include<bits/local_lim.h>

#define MAX_LINE 511
#define MAX_BUFSZ 512
#define MAX_CHAT _POSIX_THREAD_THREADS_MAX
#define MAX_SOCKET FD_SETSIZE

typedef struct map_table{
    pthread_t thid;
    char ip_addr[20];
    char port[5];
}map_t;

typedef struct addClient_pass{
    int s;
    struct sockaddr_in cliaddr;
}addClient_Pass_Types;

pthread_mutex_t count_lock;
pthread_mutexattr_t mutex_attr;

char *EXIT_STRING = "exit";
char *OUT_STRING = "OUT";
char *WHIS_STRING = "WHISPER";
char *START_STRING = "Connected to chat server\n";

char buf[MAX_LINE];
int clisock_list[MAX_CHAT-1];
map_t sock_map[MAX_CHAT-1];
int listen_sock;
int num_chat = 4;
int num = 0;
int maxfdp1;
char *user[MAX_LINE];


int clientList();
int whisperTalk(int);
void *addClient(void *);
void *outClient(void *);
void removeClient(int);
void recv_and_send(int);
void tcp_listen(int host, int port, int backlog);
pthread_t tid[MAX_CHAT -1];
int getmax();

void errquit(char *msg){
    perror(msg);
    exit(-1);
}

void thr_errquit(char *msg, int errcode){
    printf("%s\n",msg,strerror(errcode));
    pthread_exit(NULL);
}

void s_init(){
    int i;
    for(i=0; i<MAX_CHAT; i++){
        clisock_list[i] = -1;
        user[i] = NULL;
    }
}
int main(int argc, char *argv[])
{
    char buf[MAX_LINE];
    int i,port,status,nbyte,count;
    int accp_sock;
    int servlen,clilen,max_socket;
    struct sockaddr_in cliaddr;
    pthread_t tid,oid;
    addClient_Pass_Types p;
    fd_set read_fds;
    
    if(argc != 2){
        printf("사용법: %s port\n",argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);
    
    clilen = sizeof(struct sockaddr_in);
    servlen = clilen;
    s_init();
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutex_init(&count_lock,&mutex_attr);
    tcp_listen(INADDR_ANY,port,5);
    
    pthread_create(&oid,NULL,outClient,(void *)NULL);
    while(1){
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);
        
        for(i=4; i<num_chat; i++)
            FD_SET(clisock_list[i], &read_fds);
        
        maxfdp1 = getmax() + 1;
        
        if(select(maxfdp1,&read_fds,NULL,NULL,NULL) < 0)
            errquit("select fail");
        
        if(FD_ISSET(listen_sock,&read_fds)){
            accp_sock = accept(listen_sock,(struct sockaddr *)&cliaddr,&clilen);
            if(accp_sock == -1)
                errquit("accept fail\n");
            p.s = accp_sock;
            bcopy((struct sockaddr_in *)&cliaddr, &(p.cliaddr),clilen);
            status = pthread_create(&tid,NULL,&addClient,(void *)&p);
            if(status != 0)
                thr_errquit("pthread create",status);
        }
    }
    return 0;
}
void tcp_listen(int host, int port, int backlog)
{
    struct sockaddr_in servaddr;
    int accp_sock;
    
    //printf("Listen Thread : %d\n",pthread_self());
    if((listen_sock = socket(AF_INET,SOCK_STREAM,0)) < 0)
        errquit("sock fail");
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);
    
    if(bind(listen_sock,(struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        perror("bind fail");
        exit(1);
    }
    listen(listen_sock, backlog);
}
int clientList()
{
    int i;
    int flag=0;
    
    printf("-------------------------LIST-----------------------\n");
    for(i=4; i<MAX_CHAT; i++){
        if(num_chat == 4){
            printf("삭제할 클라이언트가 없습니다\n");
            break;
        }
        if(clisock_list[i] == -1 || clisock_list[i] == 0)
            continue;
        
        printf("%s : [%x:(sock:%d)]:[%s:%s]\n",user[i],sock_map[i].thid,clisock_list[i],sock_map[i].ip_addr,sock_map[i].port);
        
        flag++;
    }
    printf("-----------------------------------------------------\n",flag);
    
    return flag;
}

void *outClient(void *arg){
    int sn,flag;
    char buf[MAX_BUFSZ],sbuf[MAX_BUFSZ];
    printf("강퇴하고 싶으면 OUT을 입력하세요.\n");
    while(1){
        char bufmsg[MAX_LINE];
        fgets(bufmsg,MAX_LINE,stdin);
        if(!strcmp(bufmsg,"\n")) continue;
        else if(!strcmp(bufmsg,"OUT\n")){
            flag = clientList();
            if(flag == 0)
                continue;
            printf("강퇴할 사용자를 입력하세요. : ");
            fgets(bufmsg,strlen(bufmsg),stdin);
            bufmsg[strlen(bufmsg)-1] = '\0';
            
            for(sn=4; sn<MAX_CHAT; sn++){
                if(!strcmp(user[sn],bufmsg))
                    break;
            }
            memset(sbuf,0,sizeof(sbuf));
            if(sn <= MAX_CHAT && clisock_list[sn] != -1){
                strcat(sbuf,"you out!!!\n");
                write(sn,sbuf,MAX_BUFSZ);
                removeClient(sn);
                memset(sbuf,0,sizeof(sbuf));
                sprintf(sbuf,"user %d is out\n",sn);
                printf("%s\n",sbuf);
            }
            else
            {
                printf("Input Error\n");
                continue;
            }
        }
    }
}

void *addClient(void *called_arg)
{
    int new_index, i, tlen;
    char name[10],tbuf[32],buf[32],qbuf[32],nowbuf[32],nchat[5];
    addClient_Pass_Types *my_arg;
    
    my_arg = (addClient_Pass_Types *)called_arg;
    if(send(my_arg->s,START_STRING,strlen(START_STRING),0) == -1){
        printf("Thread(%x):START SEND ERROR\n",pthread_self());
        pthread_exit(NULL);
    }
    pthread_mutex_lock(&count_lock);
    for(i=4; i<MAX_CHAT; i++){
        if(clisock_list[i] != -1) continue;
        
        else{
            new_index = i;
            clisock_list[new_index] = my_arg -> s;
            break;
        }
    }
    
    if(i == MAX_CHAT +1){
        printf("Full Nbr of Chats\n");
        pthread_exit(NULL);
    }
    sock_map[new_index].thid = pthread_self();
    
    inet_ntop(AF_INET,&(my_arg->cliaddr.sin_addr),sock_map[new_index].ip_addr,sizeof(sock_map[new_index].ip_addr));
    sprintf(sock_map[new_index].port,"%d",ntohs(my_arg->cliaddr.sin_port));
    printf("\nnew client[%x:(sock:%d)]:[%s:%s]\n",sock_map[new_index].thid,clisock_list[new_index],sock_map[new_index].ip_addr,sock_map[new_index].port);
    
    recv(new_index,name,sizeof(name),0);
    user[new_index] = name;
    
    for(i=4; i<MAX_CHAT; i++){
        if(num_chat == 4) break;
        if(clisock_list[i] == my_arg->s || clisock_list[i] == -1)
            continue;
        strncpy(tbuf,sock_map[new_index].ip_addr,20);
        strcat(tbuf,":");
        strncat(tbuf,sock_map[new_index].port,5);
        strncpy(buf,tbuf,26);
        strncat(tbuf," in\n",5);
        strncpy(buf,tbuf,31);
        tlen = 31;
        tbuf[tlen] = 0;
        
        if(write(clisock_list[i],tbuf,strlen(tbuf)) <= 0){
            perror("write error");
            pthread_exit(NULL);
        }
    }
    num_chat++;
    num++;
    
    sprintf(nchat,"%d",num);
    for(i=4; i<MAX_CHAT; i++){
        if(num_chat == 4) break;
        if(clisock_list[i] == -1)
            continue;
        strcat(qbuf,"now chat num = ");
        strncat(qbuf,nchat,5);
        strcat(qbuf,"\n");
        strncpy(nowbuf,qbuf,32);
        tlen = 32;
        nowbuf[tlen] = 0;
        write(clisock_list[i],nowbuf,tlen);
        
        memset(&nowbuf,0,sizeof(nowbuf));
    }
    pthread_mutex_unlock(&count_lock);
    recv_and_send(my_arg->s);
}

int whisperTalk(int sock)
{
    int i,wS;
    char Msg[MAX_BUFSZ],smsg[MAX_BUFSZ],wSock[5];
    char *msg;
    
    pthread_mutex_lock(&count_lock);
    
    read(sock,Msg,MAX_BUFSZ);
    strcpy(wSock,Msg);
    char *ptr = strtok(wSock,">>");
    char wbuf[5];
    strcpy(wbuf,ptr);
    
    for(i=4; i<MAX_CHAT; i++){
        if(!strcmp(user[i],wbuf)){
            wS = i;
            break;
        }
    }
    msg = strstr(Msg,">>");
    strcpy(smsg,msg);
    
    write(clisock_list[wS],smsg,MAX_LINE);
    
    memset(&smsg,0,sizeof(smsg));
    memset(&Msg,0,sizeof(Msg));
    memset(&wSock,0,sizeof(wSock));
    
    pthread_mutex_unlock(&count_lock);
}
void recv_and_send(int sock)
{
    int i,n,alen,tlen,mlen,Ms;
    char tbuf[MAX_BUFSZ];
    
    while(n = read(sock,buf,MAX_BUFSZ) > 0){
        if(strstr(buf,WHIS_STRING) != NULL){
            whisperTalk(sock);
            continue;
        }
        
        if(strstr(buf,EXIT_STRING) != NULL){
            removeClient(sock);
            continue;
        }
        
        if(buf[0] == '['){
            mlen = strlen(buf);
            buf[mlen-1] = 0;
            for(i=4; i<= num_chat; i++){
                if(clisock_list[i] == sock || clisock_list[i] == -1)
                    continue;
                write(clisock_list[i],buf,MAX_LINE);
                alen = strlen(sock_map[sock].ip_addr);
                strncpy(tbuf,sock_map[sock].ip_addr,alen);
                strcat(tbuf,":");
                strncat(tbuf,sock_map[sock].port,6);
                strncat(tbuf,buf,n);
                tlen = alen + 6;
                tbuf[tlen] = 0;
                
                if(write(clisock_list[i],tbuf,MAX_BUFSZ) <= 0){
                    perror("write error");
                    pthread_exit(NULL);
                }
            }
            memset(&buf,0,sizeof(buf));
            memset(&tbuf,0,sizeof(tbuf));
        }
    }
}

void removeClient(int s)
{
    char tbuf[35],nowbuf[35],nchat[5];
    int i,tlen;
    pthread_mutex_lock(&count_lock);
    close(clisock_list[s]);
    
    for(i=4; i<MAX_CHAT; i++){
        if(num_chat == 4) break;
        if(clisock_list[i] == s || clisock_list[i] == -1)
            continue;
        strncpy(tbuf,sock_map[s].ip_addr,20);
        strcat(tbuf,":");
        strncat(tbuf,sock_map[s].port,5);
        strncat(tbuf," out\n",5);
        strncpy(buf,tbuf,32);
        memset(&buf,0,sizeof(buf));
        tlen = 32;
        tbuf[tlen] = 0;
        
        if(write(clisock_list[i],tbuf,strlen(tbuf)) <= 0){
            perror("write error");
            memset(&tbuf,0,sizeof(tbuf));
            printf("\n");
            pthread_exit(NULL);
        }
        tbuf[0] = '\0';
    }
    
    printf("\n[%s:%s] : out, now chat num = %d\n",sock_map[s].ip_addr,sock_map[s].port,num-1);
    
    clisock_list[s] = -1;
    sock_map[s].thid = 0;
    bzero(sock_map[s].ip_addr,sizeof(sock_map[s].ip_addr));
    bzero(sock_map[s].port,sizeof(sock_map[s].port));
    num_chat--;
    num--;
    
    sprintf(nchat,"%d",num);
    strcat(tbuf,"now chat num = ");
    strncat(tbuf,nchat,5);
    strncpy(nowbuf,tbuf,30);
    strcat(nowbuf,"\n");
    tlen = 30;
    nowbuf[tlen] = 0;
    
    for(i=4; i<MAX_CHAT; i++){
        if(num_chat ==4) break;
        if(clisock_list[i] == -1)
            continue;
        write(clisock_list[i],nowbuf,tlen);
    }
    
    pthread_mutex_unlock(&count_lock);
}

int getmax(){
    int max = listen_sock;
    int i;
    for(i=4; i<num_chat; i++)
        if(clisock_list[i] > max)
            max = clisock_list[i];
    return max;
}

