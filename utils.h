#define MAX_PKT_SIZE 1000
#define TIMEOUT 1


#define H_ACK 	1
#define H_FRAG 	2
#define H_REQ 	3
#define H_FIN	4

#define H_SIZE		(sizeof(struct header))
#define H_MAX_DATA	(MAX_PKT_SIZE - H_SIZE)
#include <stdbool.h>

struct header;

typedef struct __attribute__((packed)) header 
{
	uint32_t h_seq_num;
	uint32_t h_flag;
	uint32_t h_data_size;
	uint32_t h_offset;
} *header_t;

static void print_headers(header_t header)
{
	printf("Header: {SeqNum: %i, Flag: %i, Data: %i, Offset: %i}\n", 
		header->h_seq_num, 	header->h_flag, 
		header->h_data_size,	header->h_offset);
}

static bool trueWithProb(double prob)
{
	if (prob == 0) 	 return false;
	if (prob == 1.0) return true;
//	printf("%f\n", prob);
	return ((prob*RAND_MAX) >= rand());
}
