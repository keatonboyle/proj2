#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "util.h"

bool trueWithProb(double prob)
{
   if (prob == 0) return false;
   if (prob == 1.0) return true;
   return ((prob*RAND_MAX) >= rand());
}

char rdt_message_names[2][5] = {
   "DATA",
   " ACK"
};

char ft_message_names[3][5] = {
   "WANT",
   "  OK",
   "LAST"
};

void printRecvInfo(void *buf)
{
   printf("RECEIVING HEX DUMP\n");
   int i;
   int j;
   unsigned char *p = buf;
   for (i = 0; i < 40; i+=16)
   {
      for(j=0; j < 16; j++)
         printf("%02x ", p[i+j]);

      printf("\n");
   }
   printf("HEX DUMP---------------------------------\n");
   rdt_header_t *rdtheader = (rdt_header_t *) buf;

   printf   ("+--- RDT Packet Recieved ---+\n"
             "| Type: %s                |\n"
             "| Data Size: %06d         |\n"
             "| Sequence Number: %06d   |\n",
              rdt_message_names[rdtheader->rdt_type],
              rdtheader->rdt_dataSize,
              rdtheader->rdt_seqNum);

   if(rdtheader->rdt_dataSize != 0)
   {
      ft_header_t *ftheader = (ft_header_t *) (buf + RDT_HEADER_SIZE);

      printf("| +- CONTAINS FT PACKET -+  |\n"
             "| | Type: %s           |  |\n"
             "| | Data Size: %06d    |  |\n"
             "| +----------------------+  |\n",
             ft_message_names[ftheader->ft_type],
             ftheader->ft_dataSize);
   }

   printf("+---------------------------+\n\n");
}

void printSendInfo(void *buf)
{
   printf("SENDING HEX DUMP\n");
   int i;
   int j;
   unsigned char *p = buf;
   for (i = 0; i < 40; i+=16)
   {
      for(j=0; j < 16; j++)
         printf("%02x ", p[i+j]);

      printf("\n");
   }
   printf("HEX DUMP---------------------------------\n");

   rdt_header_t *rdtheader = (rdt_header_t *) buf;

   printf   ("+--- RDT Packet Sent -------+\n"
             "| Type: %s                |\n"
             "| Data Size: %06d         |\n"
             "| Sequence Number: %06d   |\n",
              rdt_message_names[rdtheader->rdt_type],
              rdtheader->rdt_dataSize,
              rdtheader->rdt_seqNum);

   if(rdtheader->rdt_dataSize != 0)
   {
      ft_header_t *ftheader = (ft_header_t *) (buf + RDT_HEADER_SIZE);

      printf("| +- CONTAINS FT PACKET -+  |\n"
             "| | Type: %s           |  |\n"
             "| | Data Size: %06d    |  |\n"
             "| +----------------------+  |\n",
             ft_message_names[ftheader->ft_type],
             ftheader->ft_dataSize);
   }

   printf("+---------------------------+\n\n");
}

void printCorrupt()
{
   printf("---- Corrupt Packet Recieved ----\n\n");
}

