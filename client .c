//client.c
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<signal.h>
#include<pthread.h>
#include<bits/local_lim.h>
#include<bits/select.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<netdb.h>

#define MAX_LINE 511
#define NAME_LEN 20

char *EXIT_STRING = "exit";

int tcp_connect(int af, char *servip, unsigned short port);
void errquit(char *mesg){perror(mesg); exit(1);}
void Whisper_talk(int);

char name[NAME_LEN];

int main(int argc,char *argv[]){
    char bufall[MAX_LINE+NAME_LEN], *bufmsg, tbuf[MAX_LINE],sbuf[MAX_LINE];
    int maxfdp1,s,namelen;
    fd_set read_fds;
    
    if(argc != 4){
        printf("manual : %s server_ip port name\n",argv[0]);
        exit(0);
    }
    
    sprintf(bufall,"[%s]:",argv[3]);
    
    namelen = strlen(bufall);
    bufmsg = bufall + namelen;
    s = tcp_connect(AF_INET,argv[1],atoi(argv[2]));
    if(s == -1)
        errquit("tcp_connect fail");
    
    puts("귓속말을 하려면 WHISPER를 입력하세요.");
    puts("server connect success");
    maxfdp1 = s + 1;
    FD_ZERO(&read_fds);
    
    sprintf(name,"%s",argv[3]);
    send(s,name,strlen(name),0);
    
    strcpy(tbuf,">>");
    while(1){
        FD_SET(0,&read_fds);
        FD_SET(s,&read_fds);
        if(select(maxfdp1, &read_fds, NULL, NULL, NULL)<0)
            errquit("select fail");
        
        if(FD_ISSET(s,&read_fds)){
            int nbyte;
            if((nbyte = recv(s,bufmsg,MAX_LINE,0))>0){
                strcpy(sbuf,bufmsg);
                if(strstr(sbuf,tbuf)!=NULL){
                    bufmsg[nbyte-1] = 0;
                    printf("\x1b[35m%s\n",bufmsg);
                    continue;
                }
                bufmsg[nbyte-1] = 0;
                printf("\x1b[0m%s\n",bufmsg);
            }
            if(!strcmp(bufmsg,"you out!!!\n")){
                close(s);
                exit(0);
            }
        }
        if(FD_ISSET(0,&read_fds)){
            if(fgets(bufmsg,MAX_LINE,stdin)){
                if(strstr(bufmsg,"WHISPER") != NULL){
                    send(s,bufmsg,7,0);
                    Whisper_talk(s);
                    continue;
                }
                if(send(s,bufall,namelen+strlen(bufmsg),0)<0)
                    puts("ERROR:Write error on socket");
                printf("\n");
                if(strstr(bufmsg,EXIT_STRING) != NULL){
                    puts("Good Bye");
                    close(s);
                    exit(0);
                }
            }
        }
    }
}

void Whisper_talk(int s)
{
    char sock[5],talk[MAX_LINE],sTalk[MAX_LINE];
    char sbuf[MAX_LINE];
    int sendS,tlen;
    
    fprintf(stderr,"\033[32m");
    
    printf("귓속말을 할 클라이언트를 입력하세요 : ");
    scanf("%s",sock);
    __fpurge(stdin);
    
    printf("귓속말: ");
    fgets(sbuf,MAX_LINE,stdin);
    
    strncpy(talk,sock,5);
    strcat(talk,">>");
    strcat(sTalk,talk);
    strncpy(talk,name,5);
    strcat(talk,"님이 보낸 귓속말 : ");
    strcat(sTalk,talk);
    strncpy(talk,sbuf,MAX_LINE);
    strcat(sTalk,talk);
    
    tlen = MAX_LINE + 23;
    sTalk[tlen] = 0;
    
    send(s,sTalk,tlen,0);
    
    memset(&sock,0,sizeof(sock));
    memset(&sTalk,0,sizeof(sTalk));
    memset(&talk,0,sizeof(talk));
    memset(&sbuf,0,sizeof(sbuf));
    fprintf(stderr,"\033[1;0m");
}
int tcp_connect(int af, char *servip, unsigned short port){
    struct sockaddr_in servaddr;
    int s;
    if((s = socket(af, SOCK_STREAM,0))<0)
        return -1;
    bzero((char *)&servaddr,sizeof(servaddr));
    servaddr.sin_family = af;
    inet_pton(AF_INET,servip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    
    if(connect(s,(struct sockaddr *)&servaddr,sizeof(servaddr))<0)
        return -1;
    return s;
}

