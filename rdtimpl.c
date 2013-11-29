#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <netdb.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <time.h>
#include "util.h"
#include "rdtimpl.h"
#include <stdbool.h>

int WINDOW_SIZE = 1;
double LOSS_PROBABILITY = 0;
double CORRUPT_PROBABILITY = 0;

int recvSeqNum = 0;
int sendSeqNum = 0;

uint8_t recvBuf[RDT_PACKET_MAX];
uint8_t sendBuf[RDT_PACKET_MAX];

void alarmHandler(int signum)
{
}

int rdt_recv(int sockfd, uint8_t *userbuf, int len,
             struct sockaddr *sock_addr, socklen_t *addr_len)
{
   int res; 

   rdt_header_t *header;
   void *payload;

   while (1)
   {
      error(0,0,"Waiting for another");

      res = recvfrom(sockfd, recvBuf, len+RDT_HEADER_SIZE, 0, sock_addr, addr_len);
      fprintf(stderr, "First byte recvd is a %02x\n", ((unsigned char *) recvBuf)[RDT_HEADER_SIZE+FT_HEADER_SIZE]);
      fprintf(stderr, "Last byte recvd is a %02x\n", ((unsigned char *) recvBuf)[len-1]);
      header = (rdt_header_t *) recvBuf;
      payload = recvBuf + RDT_HEADER_SIZE;

      //check for corruption
      if (trueWithProb(CORRUPT_PROBABILITY))
      {
         printCorrupt();

         //send a ACK that DOESN'T acknowledge this most recent
         // packet
         memset(recvBuf, 0, RDT_PACKET_MAX);
         header->rdt_type = RDT_TYPE_ACK;
         header->rdt_seqNum = recvSeqNum;
         header->rdt_dataSize = 0;

         fprintf(stderr, "RDT: ACKing sequence #%d.\n", recvSeqNum);
         printSendInfo(recvBuf);
         sendto(sockfd, recvBuf, RDT_HEADER_SIZE, 0, sock_addr, *addr_len);

         //and wait for a non-corrupt packet
         continue;
      }
      else if (header->rdt_seqNum + header->rdt_dataSize <= recvSeqNum) //no new data received
      {
         printRecvInfo(recvBuf);

         //send a ACK that DOESN'T acknowledge this most recent
         // packet
         memset(recvBuf, 0, RDT_PACKET_MAX);
         header->rdt_type = RDT_TYPE_ACK;
         header->rdt_seqNum = recvSeqNum;
         header->rdt_dataSize = 0;

         fprintf(stderr, "RDT: ACKing sequence #%d.\n", recvSeqNum);
         printSendInfo(recvBuf);
         sendto(sockfd, recvBuf, RDT_HEADER_SIZE, 0, sock_addr, *addr_len);

         error(0,0,"Sent");

         continue;
      }
      else
      {
         printRecvInfo(recvBuf);

         // copy the smaller of length requested and bytes
         //  received
         int toCopy = header->rdt_dataSize > len ?
                      len :
                      header->rdt_dataSize;

         error(0,0,"%d tocopy", toCopy);

         recvSeqNum += toCopy;
         error(0,0,"%d seqnum", recvSeqNum);

         //copy the payload into the application's memory
         memcpy(userbuf, payload, toCopy);

         //send a ACK that DOES acknowledge this most recent
         // packet, and return this packet
         memset(recvBuf, 0, RDT_PACKET_MAX);
         header->rdt_type = RDT_TYPE_ACK;
         header->rdt_seqNum = recvSeqNum;
         header->rdt_dataSize = 0;

         fprintf(stderr, "RDT: ACKing sequence #%d.\n", recvSeqNum);
         printSendInfo(recvBuf);
         sendto(sockfd, recvBuf, RDT_HEADER_SIZE, 0, sock_addr, *addr_len);
         
         return toCopy; //and return bytes read
      }
   }
}

int rdt_send(int sockfd, uint8_t *userbuf, int len, struct sockaddr *sock_addr, socklen_t addr_len)
{
   signal(SIGALRM, alarmHandler);
   rdt_header_t *header = (rdt_header_t *) sendBuf;
   rdt_header_t *ackheader;
   int res;
   time_t lastSent;

   header->rdt_type = RDT_TYPE_DATA;
   header->rdt_seqNum = sendSeqNum;
   header->rdt_dataSize = len;

   sendSeqNum += len;

   memcpy(sendBuf + RDT_HEADER_SIZE, userbuf, len);

   while(1)
   {
      printSendInfo(sendBuf);

      fprintf(stderr, "First byte sent is a %02x\n", ((unsigned char *) sendBuf)[FT_HEADER_SIZE+RDT_HEADER_SIZE]);
      fprintf(stderr, "Last byte sent is a %02x\n", ((unsigned char *) sendBuf)[len+RDT_HEADER_SIZE-1]);
      res = sendto(sockfd, sendBuf, len + RDT_HEADER_SIZE, 0, sock_addr, addr_len);
      alarm(0);
      alarm(3);
      siginterrupt(SIGALRM, 1);


      res = recvfrom(sockfd, recvBuf, RDT_HEADER_SIZE, 0, sock_addr, &addr_len);

      if (res != RDT_HEADER_SIZE) 
      {
         break;  //alarm went off indicating closing timeout
      }

      ackheader = (rdt_header_t *) recvBuf;

      if (trueWithProb(CORRUPT_PROBABILITY))
      {
         printCorrupt();
         continue;
      }

      if (ackheader->rdt_type == RDT_TYPE_ACK &&
           ackheader->rdt_seqNum >= sendSeqNum)
      {
         printRecvInfo(recvBuf);
         break;
      }
   }

   return len;
}
