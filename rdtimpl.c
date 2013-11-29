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

int lenLastSent = 0;
int sockLastSent = 0;
struct sockaddr *lastsock_addr = NULL;
socklen_t lastaddr_len = 0;

int retransmitCount = 0;
bool signaled = false;
bool closingTime = false;

uint8_t recvBuf[RDT_PACKET_MAX];
uint8_t sendBuf[RDT_PACKET_MAX];

void alarmHandler(int signum)
{
   signaled = true;
   if (retransmitCount != 5)
   {
      // we'll retransmit the next time through the sending loop
      retransmitCount++;
   }
   //once we're retransmitted 5 times, we assume that the problem is a failure to receive the
   // final ACK from the other side and we just will close the connection
   else
   {
      closingTime = true;
   }
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
      else if (trueWithProb(LOSS_PROBABILITY))
      {
         fprintf(stderr, "PACKET LOST\n");
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
   bool resetTimer = true;

   header->rdt_type = RDT_TYPE_DATA;
   header->rdt_seqNum = sendSeqNum;
   header->rdt_dataSize = len;

   sendSeqNum += len;

   memcpy(sendBuf + RDT_HEADER_SIZE, userbuf, len);

   while(1)
   {
      printSendInfo(sendBuf);

      res = sendto(sockfd, sendBuf, len + RDT_HEADER_SIZE, 0, sock_addr, addr_len);

      lenLastSent = len+RDT_HEADER_SIZE;
      sockLastSent = sockfd;
      lastsock_addr = sock_addr;
      lastaddr_len = addr_len;

      if (resetTimer)
      {
         alarm(0);
         alarm(1);
         siginterrupt(SIGALRM, 1);
      }


      res = recvfrom(sockfd, recvBuf, RDT_HEADER_SIZE, 0, sock_addr, &addr_len);

      if (closingTime != true) 
      {
         break;  //alarm has gone off enough times that we assume closing timeout
      }
      if (signaled)
      {
         signaled = false;
         continue; //timeout, send again
      }

      ackheader = (rdt_header_t *) recvBuf;

      if (trueWithProb(CORRUPT_PROBABILITY))
      {
         printCorrupt();
         continue;
      }

      if (trueWithProb(LOSS_PROBABILITY))
      {
         fprintf(stderr, "PACKET LOST\n");
         resetTimer = false;
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
