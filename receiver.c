/* much of the setup code has been taken from proj1 and the serverFork example */
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
#include <stdbool.h>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <time.h>
#include "util.h"
#include "rdtimpl.h"
#include <stdbool.h>

uint8_t buf[FT_PACKET_MAX];

void recvLoop(int sockfd, struct sockaddr *serv_addr, socklen_t addr_len, char *filename)
{
   int res;
   int STATE = STATE_WAITING_FOR_DATA;
   int currentFd = -1;
   int bytesWritten = 0;
   ft_header_t *ftheader;
   void *payload;

   res = rdt_recv(sockfd, buf, FT_PACKET_MAX, serv_addr, &addr_len);

   ftheader = (ft_header_t *) buf;
   payload = buf + FT_HEADER_SIZE;

   if (res != FT_HEADER_SIZE + ftheader->ft_dataSize)
   {
      error(FATAL, FATAL, "should have equal sizes");
   }


   if (ftheader->ft_type != FT_TYPE_OK &&
       ftheader->ft_type != FT_TYPE_LAST)
   {
      error(FATAL, FATAL, "should have OK or LAST");
   }

   // first byte coming through, we must create the file
   currentFd = open(filename, O_WRONLY | O_CREAT);

   if (currentFd < 0) error(FATAL, FATAL, "can't create file");


   while (1)
   {
      res = write(currentFd, payload, ftheader->ft_dataSize);

      fprintf(stderr, "%d bytes written\n", ftheader->ft_dataSize);

      if (res < ftheader->ft_dataSize) error(FATAL, FATAL, "writing to local file failed");

      if (ftheader->ft_type == FT_TYPE_LAST)
      {
         close(currentFd);
         break;
      }

      res = rdt_recv(sockfd, buf, FT_PACKET_MAX, serv_addr, &addr_len);
   }
}

int main(int nargs, char *argv[])
{
   int sockfd, newsockfd, portno, pid;
   int res;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;
   struct hostent *senderHost;
   ft_header_t ftheader;

   srand(time(NULL));
   fprintf(stderr, "Corrupt probability set here\n");

   /* argv should be:
    * [0] (program name)
    * [1] <sender_hostname>
    * [2] <sender_portno>
    * [3] <filename>
    * [4] optional <loss_probability>
    * [5] optional <corrupted_probability>
    */
   if (nargs < 4)
   {
      error(FATAL, FATAL, "wrong arguments TODO");
   }

   if (nargs >= 5)
   {
      LOSS_PROBABILITY = atof(argv[4]);
   }

   fprintf(stderr, "Corrupt probability set here\n");

   if (nargs >= 6)
   {
      CORRUPT_PROBABILITY = atof(argv[5]);
      fprintf(stderr, "Corrupt probability set to %f\n", CORRUPT_PROBABILITY);
   }

   senderHost = gethostbyname(argv[1]);

   if (senderHost == NULL)
   {
      error(FATAL, FATAL, "couldn't find host");
   }

   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0) error(FATAL, FATAL, "failure getting socket");

   serv_addr.sin_family = senderHost->h_addrtype;
   serv_addr.sin_addr =   *( (struct in_addr *) (senderHost->h_addr_list[0]));
   serv_addr.sin_port =   htons(atoi(argv[2]));

   ftheader.ft_type =     FT_TYPE_WANT;
   ftheader.ft_dataSize = strlen(argv[3]) + 1;

   memcpy(buf, &ftheader, FT_HEADER_SIZE);

   strcpy(buf + FT_HEADER_SIZE, argv[3]);

   error(0,0, "Looking for file: %s", (char *) (buf+FT_HEADER_SIZE));

   res = rdt_send(sockfd, buf, FT_HEADER_SIZE + strlen(argv[3]) + 1,
                  (struct sockaddr *) &serv_addr, sizeof(serv_addr));

   recvLoop(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr), argv[3]);
}// main
