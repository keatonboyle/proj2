/* A simple server in the internet domain using UDP
   The port number is passed as an argument 
   This version runs for 1 file transfer.
*/
#include <fcntl.h> 	//For open / close
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <sys/stat.h>
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>      //To eliminate warnings

#include "utils.h"

static struct sockaddr_in * cliref;
static int * sockref;
static int * fileref;
static uint16_t * tracker;
static int base = 0;
static int retrans_count = 0;
static size_t cwnd, last_pkt = -1, wind;

void rdt_rcv(struct sockaddr * cli_addr, int sockfd, char * buf);
void buildPacket(int fd, int id, char * buf);
void sendFIN(struct sockaddr_in cli_addr, int sockfd);
void update(uint32_t seq_num, uint32_t code);
size_t get_offset(const int id);
size_t get_size(const int it);
size_t get_num_packets(const char * filename);
size_t get_file_size(const char * filename);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void handler(int sig)
{
	if(base == last_pkt)
	{
		sendFIN(*cliref, *sockref);	
		return;
	}
	if(last_pkt != -1)
	{
		char pkt[MAX_PKT_SIZE];
		int i;
		for(i = base; i < (base + wind); i++)
		{
			if(i > last_pkt)
				continue;
			buildPacket(*fileref, i, pkt);
//			printf("Retransmit Needed\n");
			sendto(*sockref,pkt,MAX_PKT_SIZE,0,
				(struct sockaddr *)cliref,sizeof(*cliref));	
		}
	}
	retrans_count++;
	if(retrans_count >= 5){
		printf("TIMEOUT\n");
		exit(1);
	}else 
		alarm(TIMEOUT);
}

void resetAlarm()
{
	alarm(0);
	alarm(TIMEOUT);
}

int main(int argc, char *argv[])
{
     int sockfd, portno, file;
     struct sockaddr_in serv_addr, cli_addr;

     double lp, cp; 	//loss probability, corruption probability

     cliref  = & cli_addr;	//These are global pointers for loss retrans
     sockref = & sockfd;
     fileref = & file;
     base = 0;

     char pkt[MAX_PKT_SIZE];	//our generic buffer
     char * payload; 	//just pointers
     header_t header; 	//header pointer

     signal(SIGALRM, handler);

     if (argc < 2 || argc > 5) {
         fprintf(stderr,"usage %s <port> <cwnd> <p_l> <p_c>\n", argv[0]);
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_DGRAM, 0);	//create socket

     if(argc >= 3)
	cwnd = atoi(argv[2]);
     else
	cwnd = 1000; //1000 by default. Let's make it easy on ourselves.

//   printf("%i\n",cwnd);

     if(cwnd%MAX_PKT_SIZE <= H_SIZE) //Edge Condition
	cwnd -= (cwnd%MAX_PKT_SIZE);

     if(cwnd == 0){
	error("CWND too damn small!\n");
	exit(1);
     }

     wind = (cwnd/MAX_PKT_SIZE) + ((cwnd%MAX_PKT_SIZE) ? 1 : 0);

     if(argc >= 4)
	lp = atof(argv[3]);

     if(argc == 5)
	cp = atof(argv[4]);

     if (sockfd < 0) 
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
     //fill in address info
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);	//5 simultaneous connection at most
     
     while (1)
     {
	//Receive Packet

	rdt_rcv((struct sockaddr *)&cli_addr, sockfd, pkt);

	header = (header_t) pkt;
	payload = pkt + H_SIZE;
	resetAlarm();

	if(trueWithProb(lp))
	{
  		printf("ACK %i Dropped\n", header->h_seq_num);
		continue;
	}

	if(trueWithProb(cp))
	{
  		printf("ACK %i Corrupted\n", header->h_seq_num);
		retrans_count = 0; //we received something, keep alive
		continue;
	}

	print_headers(header);
	
	if(header->h_flag == H_REQ)
	{
		last_pkt = get_num_packets(payload);
		tracker = malloc((last_pkt+1)*sizeof(uint16_t));
//		printf("Number of packets to send: %i\n", last_pkt);
		if((file = open(payload,O_RDONLY))<0) 
			error("Cannot Open File"); //get fd
	}


	update(header->h_seq_num, header->h_flag);
	
	if(base > last_pkt) //we're done, finish. We've seen the last pkt acked
	{
		sendFIN(cli_addr, sockfd);
		while(header->h_flag != H_FIN)
			rdt_rcv((struct sockaddr *)&cli_addr, sockfd, pkt);
		free(tracker);
		close(file);
		break;
	}

	if(file)
	{
		int i;
		for(i = base; i < base + wind; i++)
		{
			if(tracker[i] > 0 || i > last_pkt) //we don't want previously sent/acked packets or OOB packets to send.
				continue;
			//Send Packets
	        	buildPacket(file,i,pkt);
			sendto(sockfd,pkt,MAX_PKT_SIZE,0,(struct sockaddr *)&cli_addr,sizeof(cli_addr));
			update(i, 0);
		}
	}
     } //While Loop
     return 0;
}

void rdt_rcv(struct sockaddr * cli_addr, int sockfd, char * buf)
{
    size_t x;
    bzero(buf, MAX_PKT_SIZE); //Zero our buffer
    recvfrom(sockfd,buf,MAX_PKT_SIZE,0,cli_addr,&x);
}

void buildPacket(int fd, int id, char * buf)
{
	int bytesRead;
	lseek(fd, get_offset(id), SEEK_SET); //Sets offset
	if((bytesRead=read(fd, (buf+H_SIZE), get_size(id)))<0) //writes the data into
		error("Error reading file");

	header_t header = (header_t) buf;
	header->h_flag = (id != last_pkt) ? 2 : 0;
	header->h_data_size = bytesRead;
	header->h_seq_num = id;
	header->h_offset = get_offset(id);

//	printf("%i\n", bytesRead);
}


void sendFIN(struct sockaddr_in cli_addr, int sockfd)
{
    header_t header = malloc(sizeof(struct header));
    header->h_seq_num = 0;
    header->h_flag = H_FIN;
    header->h_data_size = 0;

    sendto(sockfd,header,sizeof(struct header),0,(struct sockaddr *)&cli_addr, sizeof(cli_addr)); //Sends our ACK
    free(header);
}

void update(uint32_t seq_num, uint32_t code)
{
	if(code == H_ACK || code == H_REQ){
		retrans_count = 0;
		tracker[seq_num] = 2; //we got an ack for this packet
		while(tracker[base] == 2)
			base++; //move base forward if possible.
	}
	else
		tracker[seq_num] =1; //we just sent a packet
}

size_t get_offset(const int id)
{
	if(cwnd <= MAX_PKT_SIZE)
		return (id-1) * (cwnd - H_SIZE);
	
	size_t pkts_in_cwnd = (cwnd/MAX_PKT_SIZE) + ((cwnd%MAX_PKT_SIZE)?1:0); //ceiling(cwnd/MAX_PKT_SIZE)
	size_t diff_bytes = H_MAX_DATA - (cwnd % MAX_PKT_SIZE) + H_SIZE; 

                              //subtract diff_bytes for each full window behind this one
	return (id-1)*H_MAX_DATA - ((id-1)/pkts_in_cwnd)*diff_bytes;
}

size_t get_size(const int id)
{
	if(cwnd <= MAX_PKT_SIZE)
		return (cwnd - H_SIZE);
	
	size_t pkts_in_cwnd = (cwnd/MAX_PKT_SIZE) + ((cwnd%MAX_PKT_SIZE)?1:0);

	if(id % pkts_in_cwnd == 0)                 //corner case when packets fit perfectly in cwnd
		return (cwnd % MAX_PKT_SIZE - H_SIZE) + ((cwnd%MAX_PKT_SIZE)?0:MAX_PKT_SIZE);

	return H_MAX_DATA;
}

size_t get_num_packets(const char * filename)
{
	size_t size = get_file_size(filename);

	if(cwnd <= MAX_PKT_SIZE)
	{
		size_t data = cwnd - H_SIZE;
		return (size / data) + ((size % data) ? 1 : 0);
	}

	int p;

	for(p = 1; get_offset(p) < size; p++)
		continue;

	return (p-1);
}
size_t get_file_size(const char * filename)
{
	struct stat st;

	if(stat(filename, &st) == 0)
		return st.st_size;
	return -1;
}
