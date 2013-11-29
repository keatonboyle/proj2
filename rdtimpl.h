extern int WINDOW_SIZE;
extern double LOSS_PROBABILITY;
extern double CORRUPT_PROBABILITY;

int rdt_recv(int sockfd, uint8_t *userbuf, int len,
             struct sockaddr *sock_addr, socklen_t *addr_len);

int rdt_send(int sockfd, uint8_t *userbuf, int len, struct sockaddr *sock_addr, socklen_t addr_len);
