#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>


#define STATE_NONE 0
#define STATE_WAITING_FOR_ACK 1
#define STATE_WAITING_FOR_DATA 2
#define STATE_LAST_PACKET_SENT 4

#define FATAL 1
#define NONFATAL 0

/* different types of messages for our RDT */
#define RDT_TYPE_DATA 0
#define RDT_TYPE_ACK  1

#define RDT_HEADER_SIZE (sizeof(rdt_header_t))
#define RDT_MAX_DATA    (RDT_PACKET_MAX-RDT_HEADER_SIZE)
#define RDT_PACKET_MAX  40

struct rdt_header;

typedef struct __attribute__((packed)) rdt_header 
{
   uint32_t rdt_type;     // type (RDT_TYPE_*)
   uint32_t rdt_dataSize; // bytes of data in this packet (following this header, 
                          //  will include FT header and FT data)
   uint32_t rdt_seqNum;   // offset # of bytes
} rdt_header_t;


/* different types of messages for our File Transfer protocol */
#define FT_TYPE_WANT 0
#define FT_TYPE_OK   1
#define FT_TYPE_LAST 2

#define FT_HEADER_SIZE  (sizeof(ft_header_t))
#define FT_MAX_DATA     (FT_PACKET_MAX-FT_HEADER_SIZE)
#define FT_PACKET_MAX   RDT_MAX_DATA

struct ft_header;

typedef struct __attribute__((packed)) ft_header 
{
   uint32_t ft_type;      // type (FT_TYPE_*)
   uint32_t ft_dataSize;  // bytes of data in this packet (following this header,
                          //  will be file data or filename, etc)
} ft_header_t;

extern char rdt_message_names[2][5];
extern char ft_message_names[3][5];

void printInfo(rdt_header_t *rdtheader, ft_header_t *ftheader);

bool trueWithProb(double prob);

void printCorrupt();
#endif //UTIL_H_INCLUDED
