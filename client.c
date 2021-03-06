#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <math.h>

#define CONNECT 0
#define PUT 1
#define GET 2
#define CLOSE 3
#define CREATE 4

#define MAXARG 10
#define MAXBUF 512

typedef struct ftpPacket{
    int type;
    int len;
    char buf[1024];
}myPacket;

int id;
int isConnect = 0;
char *prompt = "client > ";
int sendrate = 10000;
int recvrate = 10000;

static char inpBuf[MAXBUF], tokBuf[2 * MAXBUF];
static char *ptr;

int strPaser(char *line, char *str[], int *strNum);
int userIn(char *p);

int runCMD(int argc, char *argv[], struct sockaddr_in *server);
int setServer(int argc, char *argv[], struct sockaddr_in *server);
int getNewSocket(int cmd, struct sockaddr_in *server);
int sendFile(char *fileName, int socket);
int receiveFile(char *fileName, int socket);
void create(char *str);

//author : Jo kwang hyeon(Karasion)
int main(int argc, char *argv[])
{
	struct sockaddr_in server;

	int numCmd;
	char *cmd[MAXARG];

	while(userIn(prompt) != EOF){
		strPaser(inpBuf, cmd, &numCmd);
		runCMD(numCmd, cmd, &server);
	}
}
//author : Jo kwang hyeon(Karasion)
int runCMD(int argc, char *argv[], struct sockaddr_in *server)
{
	int pid;
	int sock;
	myPacket pkt;
	if(strcmp(argv[0], "connect") == 0){
		if(argc != 3){
			printf("usage: %s service|port host\n", argv[0]);
			return 0;
		}
		if(isConnect != 0){
			printf("Err : Already connected!\n");
			return 0;
		}
		if(setServer(argc, argv, server) < 0)
			return 0;
		sock = getNewSocket(CONNECT, server);
		if(sock < 0)
			return 0;
		read(sock, &pkt, sizeof(pkt));
		id = pkt.type;
		isConnect = 1;
	}
	else if(strcmp(argv[0], "put") == 0){
		if(isConnect != 1){
			printf("Err : Not connected with server\n");
			return 0;
		}
		if(argc != 2){
			printf("usage: %s filename\n", argv[0]);
			return 0;
		}
		sock = getNewSocket(PUT, server);
		if(sock < 0)
			return 0;

		pid = fork();
		if(pid > 0)
			waitpid(pid);
		else{
			pid = fork();
			if(pid > 0)
				exit(0);
			else{
				sendFile(argv[1], sock);
				exit(0);
			}
		}
	}
	else if(strcmp(argv[0], "get") == 0){
		if(isConnect != 1){
			printf("Err : Not connected with server\n");
			return 0;
		}
		if(argc != 2){
			printf("usage: %s filename\n", argv[0]);
			return 0;
		}
		sock = getNewSocket(GET, server);
		if(sock < 0)
			return 0;
		
		pid = fork();
		if(pid > 0)
			waitpid(pid);
		else{
			pid = fork();
			if(pid > 0)
				exit(0);
			else{
				receiveFile(argv[1], sock);
				exit(0);
			}
		}
	}
	else if(strcmp(argv[0], "close") == 0){
		if(isConnect != 1){
			printf("Err : Not connected with server\n");
			return 0;
		}
		if(argc > 1){
			printf("usage: %s\n", argv[0]);
			return 0;
		}
		isConnect = 0;
	}
	else if(strcmp(argv[0], "quit") == 0){
		if(isConnect != 0){
			printf("Err : Connected with server\n");
			return 0;
		}
		if(argc > 1){
			printf("usage: %s\n", argv[0]);
			return 0;
		}
		printf("Bye Bye!\n");
		exit(0);
	}
	else if(strcmp(argv[0], "sendrate") == 0){
		if(argc != 2){
			printf("usage: %s [0 < send rate <= 10000]\n", argv[0]);
			return 0;
		}
		sscanf(argv[1], "%d", &sendrate);
	}
	else if(strcmp(argv[0], "recvrate") == 0){
		if(argc != 2){
			printf("usage: %s [0 < recv rate <= 10000]\n", argv[0]);
			return 0;
		}
		sscanf(argv[1], "%d", &recvrate);
	}
	else if(strcmp(argv[0], "ratecurr") == 0){
		if(argc != 1){
			printf("usage: %s\n", argv[0]);
		}
		printf("sendrate : %d K , recvrate : %d K\n", sendrate, recvrate);
	}
	else if(strcmp(argv[0], "create") == 0){
		if(argc != 2){
			printf("usage: %s [school number]\n", argv[0]);
			return 0;
		}
		create(argv[1]);
	}
	else{
		printf("Wrong CMD!\n");
		return 0;
	}
	return 1;
}
//author : Jo kwang hyeon(Karasion)
int setServer(int argc, char *argv[], struct sockaddr_in *server)
{
	struct hostent *hostp;
	struct servent *servp;
	if (isdigit(argv[1][0])) {
		static struct servent s;
		servp = &s;
		s.s_port = htons((u_short)atoi(argv[1]));
	} else if ((servp = getservbyname(argv[1], "tcp")) == 0) {
		fprintf(stderr,"%s: unknown service\n",argv[1]);
		return -1;
	}
	if ((hostp = gethostbyname(argv[2])) == 0) {
		fprintf(stderr,"%s: unknown host\n",argv[2]);
		return -1;
	}
	memset((void *) server, 0, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	memcpy((void *) &server->sin_addr, hostp->h_addr, hostp->h_length);
	server->sin_port = servp->s_port;

	return 0;
}
//author : Jo kwang hyeon(Karasion)
int getNewSocket(int cmd, struct sockaddr_in *server)
{
	myPacket pkt;
	int sock;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		return -1;
	}

	if (connect(sock, (struct sockaddr *)server, sizeof (struct sockaddr_in)) < 0) {
		(void) close(sock);
		perror("connect");
		return -1;
	}

	pkt.type = cmd;

	write(sock, &pkt, sizeof(pkt));

	return sock;
}
//author : Jo kwang hyeon(Karasion)
int sendFile(char *fileName, int socket)
{
	myPacket pkt, pkt2;
	int fd;
	int byteread, totalSend = 0;
	int num = 0;
	int nTime = 0;
	int percent = 0;
        struct stat buffer;
	struct timeval tlast;
	struct timeval tthis;
	
	pkt.type = id;
	strcpy(pkt.buf, fileName);	

	fd = open(fileName, O_RDONLY, 0644);
	if(fd < 0){
		printf("file open fail!\n");
		return -1;
	}

	if(stat(fileName,&buffer)==-1){
		printf("stat error\n");
		close(fd);
		return -1;
	}

	pkt.len = buffer.st_size;
	//send filename
	write(socket, &pkt, sizeof(pkt));
	//check err
	read(socket, &pkt, sizeof(pkt));

	if(pkt.type == 5){
		printf("server : same file exist!");
		return -1;
	}
	
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
			if((int)(((float)totalSend/(float)buffer.st_size)*100) >= percent ){
				printf("[%s] %d%% send complete!\n", 
				fileName, (int)(((float)totalSend/(float)buffer.st_size)*100));
				percent += 10;
			}
		}
		memset(&pkt, 0, sizeof(pkt));
		byteread = read(fd, pkt.buf, 1024);
		if(byteread < 0){
			printf("file read err!\n");
			return -1;
		}
		pkt.type = num++;
		totalSend += byteread;
		pkt.len = byteread;
		write(socket, &pkt, sizeof(pkt));
		while(1){
			read(socket, &pkt2, sizeof(pkt2));

			//printf("%d %d\n", pkt2.type, num);
			if(pkt2.type == num)
				break;
			write(socket, &pkt, sizeof(pkt));
		}
		if(byteread < 1024){
			break;
		}
	}

	printf("[%s] total %dK byte send complete!\n", fileName, (totalSend/1024));
	close(fd);
}
//author : Jo kwang hyeon(Karasion)
int receiveFile(char *fileName, int socket)
{
	myPacket pkt;
	int fd;
	int byteread, total = 0;
	int fileSize;
	int num = 0;
	int percent = 0;
	struct timeval tlast;
	struct timeval tthis;

	//send recvrate
	pkt.len = recvrate;
	
	pkt.type = id;
	strcpy(pkt.buf, fileName);

	//send filename
	write(socket, &pkt, sizeof(pkt));
	//check err
	read(socket, &pkt, sizeof(pkt));
	fileSize = pkt.len;

	printf("file size %d\n", fileSize);

	if(pkt.type == 5){
		printf("server : same file exist!");
		return -1;
	}	

	fd = open(fileName, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if(fd < 0){
		printf("file open fail!\n");
		return -1;
	}	

	gettimeofday(&tlast, NULL);
	while(1){
		gettimeofday(&tthis, NULL);
		if(tthis.tv_sec - tlast.tv_sec >= 1){
			tlast.tv_sec = tthis.tv_sec;
			tlast.tv_usec = tthis.tv_usec;
			if((int)(((float)total/(float)fileSize)*100) >= percent ){
				printf("[%s] %d%% recv complete!\n",
				 fileName, (int)(((float)total/(float)fileSize)*100));
				percent += 10;
			}
		}

		byteread = read(socket, &pkt, sizeof(pkt));
		if(byteread < 0){
			printf("server err!\n");
			return -1;
		}
		if(pkt.type != num){
			pkt.type = num;
			write(socket, &pkt, sizeof(pkt));
			continue;
		}
		pkt.type = num+1;
		write(socket, &pkt, sizeof(pkt));
		num++;
		total += pkt.len;
		write(fd, pkt.buf, pkt.len);
		if(pkt.len < 1024){
			break;
		}
	}

	printf("[%s] %dK byte receive complete!\n", fileName, (total/1024));
	close(fd);
}
//author : Jo kwang hyeon(Karasion)
int strPaser(char *line, char *str[], int *strNum)
{
	int num = 0;
	char *tok = tokBuf;

	while(*line != '\0'){
		*tok++ = *line++;
	}
	*tok = '\0';
	tok = tokBuf;
	while(*tok != '\0'){
		if(num > MAXARG)
			return -1;
		if(*tok == ' '){
			*tok = '\0';
			tok++;
		}
		else{
			str[num++] = tok;
			while(*tok != ' ' && *tok != '\0')
				tok++;
		}
	}
	*strNum = num;
	return 1;
}
//author : Jo kwang hyeon(Karasion)
int userIn(char *p)
{
	char c;
	int count = 0;
	
	ptr = inpBuf;
	printf("%s", p);
	
	while(1){
		if((c = getchar()) == EOF)
			return (EOF);

		if(c == '\n' && count < MAXBUF){
			inpBuf[count] = '\0';
			return count;
		}
		
		if(count < MAXBUF)
			inpBuf[count++] = c;
		if(c == '\n'){
			printf("Clinet : input line to long!\n");
			count = 0;
			printf("%s", p);
		}
	}
}

void create(char *str)
{
	int sn;
	sscanf(str, "%d", &sn);
	
	switch(sn){
		case 20103382 :
			printf("20103382 Jo kwang hyeon designed server.c & client.c and writing the report(manual)\n");
			break;
		default :
			printf("Nothing.\n");
			break;
	}
}
