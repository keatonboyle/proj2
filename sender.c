/* much of the setup code has been taken from proj1 and the serverFork example */
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include "util.h"
#include "rdtimpl.h"
#include <stdbool.h>

uint8_t buf[FT_PACKET_MAX];

void sendLoop(int sockfd)
{
   int STATE = STATE_NONE;
   ft_header_t *ftheader;
   void *payload;
   struct sockaddr cli_addr;
   socklen_t addr_len = sizeof(struct sockaddr_in);
   int currentFd;
   int res;
   off_t totalRead;

   res = rdt_recv(sockfd, buf, FT_PACKET_MAX, &cli_addr, &addr_len);

   ftheader = (ft_header_t *) buf;
   payload = buf + FT_HEADER_SIZE;

   if (ftheader->ft_type != FT_TYPE_WANT)
   {
      error(FATAL, FATAL, "should have want");
   }

   error(0, 0, "File %s requested", (char *) payload);

   // Attempt to open the file requested
   currentFd = open((char *) payload, O_RDONLY);

   if (currentFd < 0)
   {
     error(FATAL, FATAL, "can't open file");
   }

   totalRead = 0;

   while(1)
   {
      memset(buf, 0, FT_PACKET_MAX);

      // right now we can do simple sequential reads.  The read keeps our place in the file.
      // When we add windows or repeats or whatever, we'll probably need lseek or something
      //  to adjust our position in the file to read when we retransmit certain packets
      int bytesRead = read(currentFd, payload, FT_MAX_DATA);

      if (bytesRead < 0) error(FATAL, FATAL, "can't read file");

      if (bytesRead == FT_MAX_DATA)
      {
         ftheader->ft_type = FT_TYPE_OK;
      }
      else // we didn't use the full packet, this must be the last packet of the file
      {
         ftheader->ft_type = FT_TYPE_LAST;
         STATE = STATE_LAST_PACKET_SENT;
      }

      ftheader->ft_dataSize = bytesRead;

      totalRead += bytesRead;

      rdt_send(sockfd, buf, FT_HEADER_SIZE + bytesRead, 
               (struct sockaddr *) &cli_addr, addr_len);

      if(STATE & STATE_LAST_PACKET_SENT)
      {
         break;
      }
   }
}

int main(int nargs, char *argv[])
{
   int sockfd, newsockfd, portno, pid;
   int res;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;
   struct hostent *senderHost;

   srand(time(NULL));

   /* argv should be:
    * [0] (program name)
    * [1] <sender_portno>
    * [2] optional <window_size>
    * [3] optional <loss_probability>
    * [4] optional <corrupted_probability>
    */
   if (nargs < 2)
   {
      error(FATAL, FATAL, "wrong arguments TODO");
   }

   if (nargs >= 3)
   {
      WINDOW_SIZE = atoi(argv[2]);
   }

   if (nargs >= 4)
   {
      LOSS_PROBABILITY = atof(argv[3]);
   }

      fprintf(stderr, "Corrupt probability set here\n");
   if (nargs >= 5)
   {
      CORRUPT_PROBABILITY = atof(argv[4]);
      fprintf(stderr, "Corrupt probability set to %f\n", CORRUPT_PROBABILITY);
   }

   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0) error(FATAL, FATAL, "failure getting socket");

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(atoi(argv[1]));

   res = bind(sockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr));

   if (res < 0) { error(FATAL, FATAL, "||%s||", strerror(errno)); }

   sendLoop(sockfd);

}// main
