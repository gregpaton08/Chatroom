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
#include <arpa/inet.h>

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

struct c_event{
	uint16_t opcode;
	char* chatRoom;
	char stuff[PACKET_DATA_SIZE];
};

int ParseAndCreateEvent(char *input, struct c_event *eventHandle);
uint16_t get_opcode(char *recvBuf);


int main(int argc, char **argv)	{
	if(argc > 1)	{
		if(!strcmp(argv[1], "--help"))	{
			printf("Usage: %s <user name> <address> <port number> [options]\n", argv[0]);
			printf("Options:\n");
			printf("\t--help\t\tdisplay this information\n");
			printf("Commands:\n");
			printf("\t--create\t\tcreate a private chat room\n");
			printf("\t--pmsg <user name>\t\tsend a private message to a user\n");
			printf("\t--exit\t\texit the public chatroom\n");
			printf("\t--join\t\tjoin an existing chatroom\n");
			printf("\t--ls\t\tlist users\n");
			printf("\t--lsc\t\tlist chatrooms\n");
			printf("\t--lobby\t\tmove from privat chat to lobby\n");
			printf("\t--help\t\tdisplay commands\n");
			exit(0);
		}
	}

	if(argc < 4)   {
        fprintf(stderr, "Usage: %s <user name> <address> <port number> [options]\n", argv[0]);
        exit(0);
    }
	
	int sockfd;
	int port;
	struct sockaddr_in addr;
	char address[24];
	char input[1024];
	char message[1024];
	char user_name[64];
	time_t ticks;
	int i;
	int opcode;
	struct c_data data_packet;
	struct c_event event_handle;
	char recvBuf[516];

	//get port
	port = atoi(argv[3]);
	strncpy(user_name, argv[1], sizeof(user_name));

	//set up address structure
	memset((char*)&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET;  
	addr.sin_port = htons(port);
	inet_pton(AF_INET, argv[2], &addr.sin_addr);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)	{
		perror("socket");
		exit(0);
	}

	if(connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)	{
		perror("connect");
		exit(0);
	}

	//send user name to server to request to join
	struct c_connect_client packet;
	packet.opcode = htons(1);
	memcpy(packet.user_name, user_name, sizeof(packet.user_name));
	if(write(sockfd, (char *)&packet, sizeof(packet)) < 0)	{
		perror("write");
		exit(0);
	}
	printf("waiting for display connection...\n");
	if(read(sockfd, recvBuf, sizeof(recvBuf)) < 0)	{
		perror("read");
		exit(0);
	}
	if(get_opcode(recvBuf) != 6)	{
		fprintf(stderr, "could not connect\n");
		exit(0);
	}

	while(1)	{
		//clear input display
		for(i = 0; i < 50; ++i)	{
			printf("\n");
		}
		//get user input
		fgets(input, sizeof(input), stdin);
		
		//if user entered input
		if(input[0] != '\n')	{
			//ParseAndCreateEvent(input, &event_handle);
			if(!strcmp(input, "--exit\n"))	
				opcode = 4;
			else if(!strcmp(input, "--create\n"))
				opcode = 7;
			else if(!strcmp(input, "--join\n"))
				opcode = 8;
			else if(strstr(input, "--pmsg") != NULL)
				opcode = 9;
			else if(!strcmp(input, "--ls\n"))
				opcode = 10;
			else if(!strcmp(input, "--lsc\n"))
				opcode = 11;
			else if(!strcmp(input, "--lobby\n"))
				opcode = 12;
			else if(!strcmp(input, "--help\n"))
				opcode = 20;
			else	
				opcode = 3;
			
	
			//create packet
			bzero(&data_packet, sizeof(data_packet));
			data_packet.opcode = htons(opcode);
			data_packet.blocknum = htons(0);
			memcpy(data_packet.data, input, sizeof(data_packet.data));
			//send message
			if(write(sockfd, (char *)&data_packet, sizeof(data_packet)) < 0)	{
				perror("write");
				exit(0);
			}
			
			//bzero(&event_handle, sizeof(event_handle));
			
			//check for input commands
			if(!strcmp(input, "--exit\n"))	{
				break;
			}
		}
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

//parses and creates packet depending on user input.
int ParseAndCreateEvent(char *input, struct c_event *eventHandle){

	char* tempPtr;
	tempPtr = strstr(input, "--create");
	if(tempPtr != NULL ){
	//join command found create join packet
		eventHandle->opcode = htons(7);
		printf("op: %d___", eventHandle->opcode);

		eventHandle->chatRoom = tempPtr+9;
		printf("name: %s___", eventHandle->chatRoom);
		return 1;
	}
	tempPtr = strstr(input, "--join");
	if(tempPtr != NULL ){
	//join command found create join packet
		eventHandle->opcode = htons(8);
		printf("op: %d___", eventHandle->opcode);

		eventHandle->chatRoom = tempPtr+7;
		printf("name: %s___", eventHandle->chatRoom);
		return 1;
	}
	tempPtr = strstr(input, "--pmsg");
	if(tempPtr != NULL ){
	//join command found create join packet
		eventHandle->opcode = htons(9);
		printf("op: %d___", eventHandle->opcode);

		eventHandle->chatRoom = tempPtr+7;
		printf("name: %s___", eventHandle->chatRoom);
		return 1;
	}

	char temp;
	int i=0;
	return 0;
}


