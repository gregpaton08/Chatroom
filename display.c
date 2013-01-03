#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>

#define USER_NAME_SIZE 32
#define PACKET_DATA_SIZE 512

struct c_connect_client	{
	uint16_t opcode;
	char user_name[USER_NAME_SIZE];
};

struct c_data	{
	uint16_t opcode;
	uint16_t blocknum;
	char data[PACKET_DATA_SIZE];
};

uint16_t get_opcode(char *recvBuf);

int main(int argc, char **argv)	{

	if(argc != 4)   {
        fprintf(stderr, "Usage: %s <user name> <address> <port number>\n", argv[0]);
        exit(0);
    }
	
	int sockfd;
	int port;
	struct sockaddr_in addr;
	char recvBuf[1024];
	fd_set socket_set;
	int maxfd;
	char user_name[64];
	int i;

	//get port
	port = atoi(argv[3]);
	strncpy(user_name, argv[1], sizeof(user_name));

	//set up address structure
	memset((char*)&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET;  
	addr.sin_port = htons(port);
	//addr.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET, argv[2], &addr.sin_addr);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)	{
		perror("socket");
		exit(0);
	}

	if(connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)	{
		perror("connect");
		exit(0);
	}

	struct c_connect_client packet;
	packet.opcode = htons(2);
	memcpy(packet.user_name, user_name, sizeof(packet.user_name));
	if(write(sockfd, (char *)&packet, sizeof(packet)) < 0)	{
		perror("write");
		exit(0);
	}

	if(read(sockfd, recvBuf, sizeof(recvBuf)) < 0)	{
		perror("read");
		exit(0);
	}
	if(get_opcode(recvBuf) != 6)	{
		fprintf(stderr, "could not connect\n");
		close(sockfd);
		exit(0);
	}

	FD_ZERO(&socket_set);
	FD_SET(sockfd, &socket_set);
	maxfd = sockfd;

	//clear output display
	for(i = 0; i < 50; ++i)	{
		printf("\n");
	}

	while(1)	{
		if(select(maxfd + 1, NULL, &socket_set, NULL, NULL) < 0) {
            perror("select");
            exit(0);
        }	
	
		if(FD_ISSET(sockfd, &socket_set)) {
			if(read(sockfd, recvBuf, sizeof(recvBuf)) < 0)	{
				perror("write");
				exit(0);
			}
		}
		if(!strcmp(recvBuf, "--exit"))	{
			break;
		}
		printf("%s", recvBuf);
		fflush(stdout);
	}

	close(sockfd);
	
	return 0;
}

uint16_t get_opcode(char *recvBuf)	{
	int i;
	uint16_t op = 0;
    uint16_t byte1;
	uint16_t byte0;
	char opcode[2];

	//parse the packet
    for(i = 0; i < 2; ++i)  {
        if(i < 2)   {
            opcode[i] = recvBuf[i];
        }
    }

	//convert opcode from char array to uint16_t
    byte0 = opcode[1];
	byte1 = opcode[0];
    byte1 = byte1 << 8;
	op = byte0 | byte1;

	//check opcode is valid
	return op;	
}
