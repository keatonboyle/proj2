/*
 * A simple client in the internet domain using UDP
 * Usage: ./client <hostname> <port> <filename> (./client 192.168.0.151 10000 example.txt)
 */
#include <fcntl.h> //for open/close
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <string.h>
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>      //To eliminate warnings
#include <time.h>

#include "utils.h"


void rdt_rcv(struct sockaddr_in serv_addr, int sockfd, char * buf);
void writePacket(int fd, int id, const char * buf);
void sendACK(struct sockaddr_in server, int sockid, int id);
void sendREQ(struct sockaddr_in server, int sockid, const char * filename);
void sendFIN(struct sockaddr_in server, int sockid);

void error(char *msg)
{
    perror(msg);
    exit(0);
}

static struct sockaddr_in * ref1;
static int * ref2;
static char ** ref3;
static int retrans_count = 0;

void handler(int sig)
{
    sendREQ(*ref1, *ref2, *ref3);
    retrans_count++;
    if(retrans_count >= 5){
	printf("TIMEOUT\n");
	exit(0);
    }
    alarm(TIMEOUT);
}


int main(int argc, char *argv[])
{
    int sockfd, file, portno, last_pkt_rcv;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    char pkt[MAX_PKT_SIZE];
    float lp = 0, cp = 0;
    header_t header;

    srand(time(NULL));

    if (argc < 4 || argc > 6) {
       fprintf(stderr,"usage %s <hostname> <port> <filename> <p_l> <p_c>\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //Sets up server info for client. Don't care about how it works.
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    if (argc >= 5)
	lp = atof(argv[4]);
    if (argc == 6)
	cp = atof(argv[5]);
   
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);


    ref1 = & serv_addr;
    ref2 = & sockfd;
    ref3 = & argv[3];
    signal(SIGALRM, handler); 

    // Create output path, directory and file.

    char path[MAX_PKT_SIZE];
    bzero(path, MAX_PKT_SIZE);
    strcpy(path, "./output");

    mkdir(path, 0);
    chmod(path, 0777);

    strncat(path, "/", 1);
    strncat(path, argv[3], H_MAX_DATA);
    file = open(path,O_WRONLY|O_CREAT, 0666);


    
    //State Machine
    while (1) {
	if(!header || header->h_seq_num == 0)
	{
		sendREQ(serv_addr, sockfd, argv[3]);
		alarm(TIMEOUT);
	}
	else
		sendACK(serv_addr, sockfd, last_pkt_rcv);

listen:
	rdt_rcv(serv_addr, sockfd, pkt);

	header = (header_t) pkt; 

	if(trueWithProb(cp)) //Send a dup ACK and wait.
	{
  		printf("Packet %i Corrupted\n", header->h_seq_num);
		continue;
	}
	if(trueWithProb(lp)) //Wait.
	{
  		printf("Packet %i Dropped\n", header->h_seq_num);
		goto listen;
	}

   printf("Recieved w/ ");
	print_headers(header);

	if(header->h_seq_num == 1) //Gotta get rid of that retrans timeout
		alarm(0);

	if(header->h_flag == H_FIN)
	{
      alarm(0);
		sendFIN(serv_addr, sockfd);
		close(file);
		exit(0);
	}
	
	if(header->h_seq_num == last_pkt_rcv + 1)
	{
		last_pkt_rcv++;
		writePacket(file, header->h_seq_num, pkt);
	}
    }

    return 0;
}

void rdt_rcv(struct sockaddr_in serv_addr, int sockfd, char * buf)
{
    bzero(buf, MAX_PKT_SIZE); //Zero our buffer
    recvfrom(sockfd,buf,MAX_PKT_SIZE,0,0,0);
}

void writePacket(int fd, int id, const char * buf)
{
   header_t header = (header_t) buf;
   lseek(fd, header->h_offset, SEEK_SET); //Sets offset
   if(write(fd, (buf+H_SIZE), header->h_data_size)<0)
	error("Error writing to file");
}

//ACKs packet ID
void sendACK(struct sockaddr_in serv_addr, int sockfd, int id)
{
    header_t header = malloc(sizeof(struct header));
    header->h_seq_num = id;
    header->h_flag = H_ACK;
    header->h_data_size = 0;

    printf("Sending w/ ");
    print_headers(header);
    sendto(sockfd,header,sizeof(struct header),0,(struct sockaddr *)&serv_addr, sizeof(serv_addr)); //Sends our ACK
    free(header);
}

void sendREQ(struct sockaddr_in serv_addr, int sockfd, const char * filename)
{
//  Create + send request packet
    char buf[MAX_PKT_SIZE];

    bzero(buf, MAX_PKT_SIZE);

    header_t header = (header_t)malloc(H_SIZE);
    header->h_seq_num = 0;
    header->h_flag = H_REQ;
    header->h_data_size = strlen(filename);
    
    memcpy(buf, header, H_SIZE);
    memcpy(buf+H_SIZE, filename, H_MAX_DATA-1);

    printf("Sending w/ ");
    print_headers(header);
    sendto(sockfd,buf,(H_SIZE + strlen(filename)),0,(struct sockaddr *)&serv_addr, sizeof(serv_addr));
}
void sendFIN(struct sockaddr_in serv_addr, int sockfd)
{
    header_t header = malloc(sizeof(struct header));
    header->h_seq_num = 0;
    header->h_flag = H_FIN;
    header->h_data_size = 0;

    printf("Sending w/ ");
    print_headers(header);
    sendto(sockfd,header,sizeof(struct header),0,(struct sockaddr *)&serv_addr, sizeof(serv_addr)); //Sends our ACK
    free(header);
}
