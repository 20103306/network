#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#define CONNECT 0
#define PUT 1
#define GET 2
#define CLOSE 3

typedef struct ftpPacket{
    int type;
    int len;
    char buf[1024];
}myPacket;

int clientID = 1;

int getCMD(int socket);
int runCMD(int cmd, int socket);
int sendFile(int socket);
int receiveFile(int socket);
//author : Jo kwang hyeon(Karasion)
int main(int argc, char *argv[])
{
	myPacket pkt;
	struct servent *servp;
	struct sockaddr_in server, remote;
	int request_sock, new_sock;
	int bytesread, addrlen;
	int cmd, pid;


	char buf[BUFSIZ];
	if (argc != 2) {
		(void) fprintf(stderr,"usage: %s service|port\n",argv[0]);
		exit(1);
	}
	if ((request_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		exit(1);
	}
	if (isdigit(argv[1][0])) {
		static struct servent s;
		servp = &s;
		s.s_port = htons((u_short)atoi(argv[1]));
	} else if ((servp = getservbyname(argv[1], "tcp")) == 0) {
		fprintf(stderr,"%s: unknown service\n", "tcp");
		exit(1);
	}
	memset((void *) &server, 0, sizeof server);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = servp->s_port;
	if (bind(request_sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(request_sock, SOMAXCONN) < 0) {
		perror("listen");
		exit(1);
	}

	addrlen = sizeof(struct sockaddr_in);
	while (1) {
		new_sock = accept(request_sock,
				(struct sockaddr *)&remote, (socklen_t*) &addrlen);
		if (new_sock < 0) {
			perror("accept");
			exit(1);
		}
		cmd = getCMD(new_sock);
		
		if(cmd == CONNECT){
		printf("connection from host %s, port %d, socket %d\n",
			inet_ntoa(remote.sin_addr), ntohs(remote.sin_port),
			new_sock);
		}
		
		pid = fork();
		if(pid > 0){
			waitpid(pid, NULL, 0);
			//close(new_sock);
		}
		else{
			pid = fork();
			if(pid > 0)
				exit(0);
			else{
				runCMD(cmd, new_sock);
				exit(0);
			}
		}
		
	}
}
//author : Jo kwang hyeon(Karasion)
int getCMD(int socket)
{
	myPacket pkt;
	read(socket, &pkt, sizeof(pkt));
	switch(pkt.type){
		case CONNECT :
			pkt.type = clientID++;
			write(socket, &pkt, sizeof(pkt));
			return CONNECT;
		case PUT :
			return PUT;
		case GET :
			return GET;
		case CLOSE :
			return CLOSE;
	}
}
//author : Jo kwang hyeon(Karasion)
int runCMD(int cmd, int socket)
{
	switch(cmd){
		case CONNECT : break;
		case PUT : receiveFile(socket); break;
		case GET : sendFile(socket); break;
		case CLOSE : printf("Client disconnect!\n"); break;
	}
}
//author : Jo kwang hyeon(Karasion)
int sendFile(int socket)
{
	myPacket pkt, pkt2;
	int fd, id;
	int byteread, total=0;
	char fileName[100];
	struct stat buffer;
	int num = 0;
	int nTime = 0;
	int sendrate;
	struct timeval tlast;
	struct timeval tthis;
	
	read(socket, &pkt, sizeof(pkt));
	id = pkt.type;
	strcpy(fileName, pkt.buf);
	sendrate = pkt.len;
	
       	printf("put %s [%d] to client %d\n", fileName, pkt.len, id);
	fd = open(fileName, O_RDONLY, 0644);
	if(fd < 0){
		printf("file open fail!\n");
		pkt.type = 5;
		write(socket, &pkt, sizeof(pkt));
		return -1;	
	}

	if(stat(fileName,&buffer)==-1){
		printf("stat error\n");
		close(fd);
		return -1;
	}

	pkt.type = 0;
	pkt.len = buffer.st_size;
	write(socket, &pkt, sizeof(pkt));

	gettimeofday(&tlast, NULL);
	while(1){
		gettimeofday(&tthis, NULL);
		if(tthis.tv_sec - tlast.tv_sec < 1){
			if((num-nTime) > sendrate){
				//printf("sleep\n");
				usleep(1000000 - abs(tthis.tv_usec - tlast.tv_usec));
				//printf("awake\n");
			}
		}
		else {
			nTime = num;
			tlast.tv_sec = tthis.tv_sec;
			tlast.tv_usec = tthis.tv_usec;
			printf("send : to client %d [%s] %d%% send complete.\n", id,
				 fileName, (int)(((float)total/(float)buffer.st_size)*100));
		}
		memset(&pkt, 0, sizeof(pkt));
		byteread = read(fd, pkt.buf, 1024);
		if(byteread < 0){
			printf("file read err!\n");
			return -1;
		}
		pkt.type = num++;
		total += byteread;
		pkt.len = byteread;
		write(socket, &pkt, sizeof(pkt));
		while(1){
			read(socket, &pkt2, sizeof(pkt2));
			if(pkt2.type == num)
				break;
			write(socket, &pkt, sizeof(pkt));
		}
		if(byteread < 1024)
			break;
	}
	printf("send : to client %d [%s] total %dK byte receive complete!\n", id, fileName, (total/1024));
	close(fd);
	return 0;
}
//author : Jo kwang hyeon(Karasion)
int receiveFile(int socket)
{
	myPacket pkt;
	int fd, id;
	int byteread, total=0;
	char fileName[100];
	int fileSize;
	int num = 0;
	struct timeval tlast;
	struct timeval tthis;
	
	read(socket, &pkt, sizeof(pkt));
	id = pkt.type;
	strcpy(fileName, pkt.buf);
	fileSize = pkt.len;
	
	pkt.type = 0;
	
	fd = open(fileName, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if(fd < 0){
		printf("file open fail!\n");
		pkt.type = 5;
		write(socket, &pkt, sizeof(pkt));
		return -1;	
	}
	write(socket, &pkt, sizeof(pkt));
	
	gettimeofday(&tlast, NULL);
	while(1){
		gettimeofday(&tthis, NULL);
		if(tthis.tv_sec - tlast.tv_sec >= 1){
			tlast.tv_sec = tthis.tv_sec;
			tlast.tv_usec = tthis.tv_usec;
			printf("recv : from client %d [%s] %d%% recv complete.\n", id,
				 fileName, (int)(((float)total/(float)fileSize)*100));
		}

		memset(&pkt, 0, sizeof(pkt));
		byteread = read(socket, &pkt, sizeof(pkt));
		if(byteread < 0){
			printf("client%d err!\n", id);
			return -1;
		}
		
		if(pkt.type != num){
			pkt.type = num;
			write(socket, &pkt, sizeof(pkt));
			//printf("      %d %d %d\n", pkt.type, pkt.len, num);
			continue;
		}
		//printf("%d %d %d\n", pkt.type, pkt.len, num);
		pkt.type = num+1;
		write(socket, &pkt, sizeof(pkt));
		num++;
		total += pkt.len;
		write(fd, pkt.buf, pkt.len);
		if(pkt.len < 1024)
			break;
	}
	printf("recv : from client %d [%s] total %dK byte receive complete!\n", id, fileName, (total/1024));
	close(fd);
	return 0;
}
