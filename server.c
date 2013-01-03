/*
Network Centric Programming
Final Project
Greg Paton
Chris Jelesnianski
Work was distributed evenly
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>    
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 

//constants
#define USER_NAME_SIZE 64
#define PACKET_DATA_SIZE 512
#define MAX_NUM_USERS 64
#define MAX_NUM_CHATS 16

//structs
struct user	{
	char username[USER_NAME_SIZE];
	int clientfd;
	int displayfd;
	int set;
};

struct chatroom	{
	char name[USER_NAME_SIZE];
	char password[32];
	//struct user *users;
	struct user users[MAX_NUM_USERS];
	int set;
};

struct c_event{
	uint16_t opcode;
	char* chatRoom;
	char stuff[PACKET_DATA_SIZE];
};

struct event	{
	uint16_t opcode;
	char message[512];
};

//globals
static struct chatroom chatrooms[MAX_NUM_CHATS];

//function prototypes
uint16_t get_opcode(char *recvBuf);
int get_user_name(char *recvBuf, int size, char *user_name);
int get_data(char *recvBuf, int size, char *data);
int createChatRoom(int clientfd, int displayfd);
void joinChatRoom(struct user *public_user);
int get_user_chatroom(int clientfd, struct chatroom *chatrooms);
int send_formatted_message(int clientfd, int displayfd, char *message, struct user *public_users);
int send_formatted_message_chat(int clientfd, int displayfd, char *recvBuf, struct chatroom chatroom);
int send_pmsg(char *recvBuf, int size, int clientfd, struct user *public_users);
int send_pmsg_chat(char *recvBuf, int size, int clientfd, struct chatroom *chatrooms);
int notify_user_exit(int clientfd, struct user *public_users);
int notify_user_exit_chat(int clientfd, struct chatroom chatroom);
int notify_user_enter_chat(int clientfd, struct chatroom chatroom);
void leaveChat(int clientfd, struct user *public_users);
int print_address();
int print_help_info(int displayfd);

int main(int argc, char **argv)	{
	if(argc != 2)   {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }
	
	int listenfd, connfd;
	int port;
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t length;
	char message[4096];
	char recvBuf[1024];
	fd_set socket_set, temp_set;
	fd_set client_set, display_set;
	int max_clientfd = 0;
	int max_displayfd = 0;
	int maxfd = 0;
	int nready;
	int i, j, k, l;
	int tempfd;
	time_t ticks;
	struct user public_users[MAX_NUM_USERS];	
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		public_users[i].set = 0;
	}
	uint16_t opcode;
	char user_name[64];
	char data[512];
	struct event error_packet;
	error_packet.opcode = htons(0);
	struct event ack_packet;
	int found;
	int chatroom_index;
	char name[64];

	//initialize chatrooms
	for(i = 0; i < MAX_NUM_CHATS; ++i)	{
		chatrooms[i].set = 0;
		for(j =0; j < MAX_NUM_USERS; ++j)	{
			chatrooms[i].users[j].set = 0;
		}
	}

	//get port
	port = atoi(argv[1]);

	//set up address structure
	memset((char*)&serverAddr, 0, sizeof(serverAddr)); 
	serverAddr.sin_family = AF_INET;  
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	//create socket
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)	{
		perror("socket");
		exit(0);
	}

	//bind socket
	if(bind(listenfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)	{
		perror("bind");
		exit(0);
	}	

	//set socket to listen
	if(listen(listenfd, 5) < 0)	{
		perror("listen");
		exit(0);
	}

	length = sizeof(clientAddr);
	FD_ZERO(&client_set);
	FD_SET(listenfd, &client_set);
	max_clientfd = listenfd;

	print_address();

	while(1)	{
		temp_set = client_set;

		if(select(max_clientfd + 1, &temp_set, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(0);
        }	

		//loop through all file descriptors
		for(i = 0; i <= max_clientfd; ++i)	{
			found = 0;
			if(FD_ISSET(i, &temp_set)) {				
				//if new connection
				if(i == listenfd)	{					
					//accept new connection
					if((tempfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length)) < 0)	{
						perror("accept");
						continue;
					}
					if(read(tempfd, recvBuf, sizeof(recvBuf)) < 0)	{
						perror("read");
						exit(0);
					}
					opcode = get_opcode(recvBuf);
					get_user_name(recvBuf, sizeof(recvBuf), user_name);
					//if display is connecting
					if(opcode == 2)	{
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							if(!strcmp(public_users[j].username, user_name))	{
								found = 1;
								public_users[j].displayfd = tempfd;
								FD_SET(tempfd, &display_set);
								//send acknowledgement to display
								ack_packet.opcode = htons(6);
								if(write(public_users[j].displayfd, (char *)&ack_packet, sizeof(ack_packet)) < 0)	{
									perror("write");
									exit(0);
								}
								//send acknowledgement to client
								if(write(public_users[j].clientfd, (char *)&ack_packet, sizeof(ack_packet)) < 0)	{
									perror("write");
									exit(0);
								}
								//update maxfd
								if(tempfd > max_displayfd)
									max_displayfd = tempfd;
								break;
							}
						}
						if(!found)	{
							close(tempfd);
							continue;
						}
					}
					//else if client is connecting
					else if(opcode == 1)	{
						FD_SET(tempfd, &client_set);
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							if(public_users[j].set == 0)	{
								memcpy(public_users[j].username, user_name, sizeof(public_users[j].username));
								public_users[j].clientfd = tempfd;
								public_users[j].set = 1;								
								//update maxfd
								if(tempfd > max_clientfd)	
									max_clientfd = tempfd;
								break;
							}
						}
					}
					//else send error
					else	{
						if(write(i, (char *)&error_packet, sizeof(error_packet)) < 0)	{
							perror("write");
							exit(0);
						}
						close(tempfd);
					}
				}
				//else, handle current connection
				else	{					
					//read from client
					if(read(i, recvBuf, sizeof(recvBuf)) < 0)	{
						perror("read");
						exit(0);
					}
					opcode = get_opcode(recvBuf);
					//if data received
					if(opcode == 3)	{
						//check if user is in chatroom
						if((chatroom_index = get_user_chatroom(i, chatrooms)) != -1)	{
							//loop through all users in chatroom
							for(j = 0; j < MAX_NUM_USERS; ++j)	{
								//if user is active, display message to them
								if(chatrooms[chatroom_index].users[j].set)	{
									send_formatted_message_chat(i, chatrooms[chatroom_index].users[j].displayfd, recvBuf,chatrooms[chatroom_index]);
								}
							}
						}
						//else write to all public clients
						else	{
							for(j = 0; j <= max_displayfd; ++j)	{
								if(FD_ISSET(j, &display_set)) {
									//check if user is a public user
									for(k = 0; k < MAX_NUM_USERS; ++k)	{
										if(public_users[k].displayfd == j)	{
											send_formatted_message(i, j, recvBuf, public_users);
										}
									}
								}
							}
						}
					}	
					//if client is exiting
					else if(opcode == 4)	{
						//find user
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							//check if public user
							if(public_users[j].clientfd == i)	{
								//remove user from sets and public_users
								notify_user_exit(i, public_users);
								write(public_users[j].displayfd, "--exit", 7);
								close(public_users[j].clientfd);
								close(public_users[j].displayfd);
								FD_CLR(public_users[j].clientfd, &client_set);
								FD_CLR(public_users[j].displayfd, &display_set);
								bzero((char *)&public_users[j], sizeof(public_users[j]));								
								break;
							}
							for(k = 0; k < MAX_NUM_CHATS; ++k)	{
								if(chatrooms[k].users[j].clientfd == i)	{
									//remove user from sets and public_users
									notify_user_exit_chat(i, chatrooms[k]);
									write(chatrooms[k].users[j].displayfd, "--exit", 7);
									close(chatrooms[k].users[j].clientfd);
									close(chatrooms[k].users[j].displayfd);
									FD_CLR(chatrooms[k].users[j].clientfd, &client_set);
									FD_CLR(chatrooms[k].users[j].displayfd, &display_set);
									bzero((char *)&chatrooms[k].users[j], sizeof(chatrooms[k].users[j]));
									break;
								}
							}
						}
					}
					//create chatroom
					else if(opcode == 7){  
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							if(public_users[j].clientfd == i)	{
								createChatRoom(public_users[j].clientfd, public_users[j].displayfd); 
								break;
							}
						}
					}
					//joinchatroom
					else if(opcode == 8){  
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							if(public_users[j].clientfd == i)	{
								joinChatRoom(&public_users[j]);
								break;
							}
						}
					}
					//if pmsg command called
					else if(opcode == 9)	{
						//get users displayfd
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							//check public users
							if((public_users[j].clientfd == i) && (public_users[j].set != 0))	{
								//send private message
								if(send_pmsg(recvBuf, sizeof(recvBuf), i, public_users) < 0)	{
									if(write(public_users[j].displayfd, "--pmsg failed\n", sizeof("--pmsg failed\n")) < 0)	{
										perror("write");
										break;
									}
								}
								found = 1;
							}
							//check private chat users
							for(k = 0; k < MAX_NUM_CHATS; ++k)	{
								if((chatrooms[k].users[j].clientfd == i) && (chatrooms[k].users[j].set !=0))	{
									if(send_pmsg_chat(recvBuf, sizeof(recvBuf), i, chatrooms) < 0)	{
										if(write(chatrooms[k].users[j].displayfd, "--pmsg failed\n", sizeof("--pmsg failed\n")) < 0)	{
											perror("write");
											break;
										}
									}
									found = 1;
									break;
								}
							}
							if(found)
								break;
						}		
					}
					//if ls command called
					else if(opcode == 10)	{
						//get user displayfd
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							//check if user is public
							if(public_users[j].clientfd == i)	{
								//loop through all users and print usernames
								for(k = 0; k < MAX_NUM_USERS; ++k)	{
									if(public_users[k].set)	{
										bzero(&message, sizeof(message));
										snprintf(message, sizeof(message), "%s\n", public_users[k].username);
										if(write(public_users[j].displayfd, message, sizeof(message)) < 0)	{
											perror("write");
											continue;
										}
									}
								}
								break;
							}
							//check if user is private
							for(k = 0; k < MAX_NUM_CHATS; ++k)	{
								if(chatrooms[k].users[j].clientfd == i)	{
									//loop through all users and print usernames
									for(l = 0; l < MAX_NUM_USERS; ++l)	{
										if(chatrooms[k].users[l].set)	{
											bzero(&message, sizeof(message));
											snprintf(message, sizeof(message), "%s\n", chatrooms[k].users[l].username);
											if(write(chatrooms[k].users[j].displayfd, message, sizeof(message)) < 0)	{
												perror("write");
												continue;
											}
										}
									}
									break;
								}	
							}
						}
					}
					//if lsc command called
					else if(opcode == 11)	{
						//get user displayfd
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							if(public_users[j].clientfd == i)	{
								//find active chatrooms
								for(k = 0; k < MAX_NUM_CHATS; ++k)	{
									//if chatroom is active, write to user displayfd
									if(chatrooms[k].set != 0)	{
										bzero(&message, sizeof(message));
										snprintf(message, sizeof(message), "%s", chatrooms[k].name);
										if(write(public_users[j].displayfd, message, sizeof(message)) < 0)	{
											perror("write");
											continue;
										}
									}
								}
								break;
							}							
						}
					}	
					//if lobby command called
					else if(opcode == 12)	{
						leaveChat(i, public_users);
					}		
					//if help command called
					else if(opcode == 20)	{
						//loop through all users
						for(j = 0; j < MAX_NUM_USERS; ++j)	{
							//check public users
							if(public_users[j].clientfd == i)	{
								print_help_info(public_users[j].displayfd);
								found = 1;
							}
							//check private chat users
							for(k = 0; k < MAX_NUM_CHATS; ++k)	{
								if(chatrooms[k].users[j].clientfd == i)	{
									print_help_info(chatrooms[k].users[j].displayfd);
									found = 1;
									break;
								}
							}
							if(found)
								break;
						}
					}
				}
			}
		}	
	}

	close(connfd);
	close(listenfd);
	
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

int get_user_name(char *recvBuf, int size, char *user_name)	{
	int i = 0;
	char curr;
	do	{
		curr = recvBuf[i+2];
		user_name[i] = curr;
		++i;
	} while(curr != 0);

	return 1;
}

int get_data(char *recvBuf, int size, char *data)	{
	int i = 0;
	char curr;
	do	{
		curr = recvBuf[i+4];
		data[i] = curr;
		++i;
	} while(curr != 0);

	return 1;
}

int createChatRoom(int clientfd, int displayfd){
	char ask[] = "Create chatroom name:  (\"--cancel\" to cancel)\n";
	char ask2[] = "Create chatroom password:\n";
	char ask3[] = "Re-enter password:\n";
	char err[] = "Name already in use. Choose new name: (\"--cancel\" to cancel) \n";
	char err2[] = "Passwords do not match! Re-enter password:\n";
	char ack[]= "ChatRoom has been created.\n";
	char cancel[] = "Action cancelled\n";
	char name[64];
	char password[64];
	char temp[64];
	int len;
	int n;
	int nread;
	int open_index;
	int same = 1;

	//find available chatroom in array
	for(n = 0; n < MAX_NUM_CHATS; ++n)	{
		if(!chatrooms[n].set)	{
			open_index = n;
			break;
		}
	}
	//ask user for chatroom name
	if(write(displayfd, ask, sizeof(ask)) < 0){
		perror("write");
		exit(0);
	}
	
	while(same)	{
		same = 0;
		//read name from client
		bzero(&name, sizeof(name));
		if(read(clientfd, name, sizeof(name)) < 0)	{
			perror("read");
			return -1;
		}
		//if action canceled by user
		if(!strcmp(name+4, "--cancel\n")){  
			if(write(displayfd, cancel, sizeof(cancel)) < 0){
				perror("write");
				exit(0);
			}
			return 0;
		}
		//check if name is available
		for(n = 0; n < MAX_NUM_CHATS; ++n){
			if((!strcmp(name+4, chatrooms[n].name)) && (chatrooms[n].set != 0)){
				same = 1;
				if(write(displayfd, err, sizeof(err)) < 0){
					perror("write");
					return -1;
				}
				break;
			}
		}
	}
	//set chatroom name
	memcpy(chatrooms[open_index].name, name+4, sizeof(chatrooms[open_index].name));
	//ask user to set password
	if(write(displayfd, ask2, sizeof(ask2)) < 0){
		perror("write");
		exit(0);
	}
	while(1)	{		
		//read password from client
		bzero(&temp, sizeof(temp));
		while(get_opcode(temp) != 3)	{
			if(read(clientfd, temp, sizeof(temp)) < 0)	{
				perror("read");
				exit(0);
			}		
		}
		//if action canceled by user
		if(!strcmp(temp+4, "--cancel\n")){  
			if(write(displayfd, cancel, sizeof(cancel)) < 0){
				perror("write");
				exit(0);
			}
			return 0;
		}
		memcpy(password, temp+4, sizeof(password));
		//ask user to re-enter password
		if(write(displayfd, ask3, sizeof(ask3)) < 0){
			perror("write");
			exit(0);
		}
		//read password again
		bzero(&temp, sizeof(temp));
		while(get_opcode(temp) != 3)	{
			if(read(clientfd, temp, sizeof(temp)) < 0)	{
				perror("read");
				exit(0);
			}
		}
		//if action canceled by user
		if(!strcmp(temp+4, "--cancel\n")){  
			if(write(displayfd, cancel, sizeof(cancel)) < 0){
				perror("write");
				exit(0);
			}
			return 0;
		}
		if(!strcmp(temp+4, password))	{
			break;
		}
		if(write(displayfd, err2, sizeof(err2)) < 0){
			perror("write");
			exit(0);
		}
	}
	//set chatroom password
	memcpy(chatrooms[open_index].password, password, sizeof(chatrooms[open_index].password));
	//set chatroom as active
	chatrooms[open_index].set = 1;
	//send confirmation
	if(write(displayfd, ack, sizeof(ack)) < 0){
		perror("write");
		exit(0);
	}
	return 0;
} //end createChatRoom

void joinChatRoom(struct user *public_user){
	char ask[]= "What Chat Room would you like to join: (\"--cancel\" to cancel)\n";
	char ack1[128];
	char ack2[]= "Joining chatroom failed.\n";
	char ack3[]= "Password incorrect. Returning to public chatroom.\n";
	char askpass[]= "Password: \n";
	char name[64];
	char password[64];
	bzero(&password, sizeof(password));
	int i, j, k;
	int attempts = 0;
	int open_index;
	int chatroom_index;

	
	//ask user which chatroom to join	
	if(write(public_user->displayfd, ask, sizeof(ask)) < 0){
		perror("write");
		exit(0);
	}
	//get chatroom name
	if(read(public_user->clientfd, name, sizeof(name)) < 0)	{
		perror("read");
		exit(0);
	}
	//check if name exists
	for(i = 0; i < MAX_NUM_CHATS; ++i){
		if((!strcmp(name+4, chatrooms[i].name)) && (chatrooms[i].set != 0)){
			chatroom_index = i;
			//find available user in chatroom array
			for(j = 0; j < MAX_NUM_CHATS; ++j)	{
				if(!chatrooms[i].users[j].set)	{
					open_index = j;
					break;
				}
			}
			//ask user for password
			if(write(public_user->displayfd, askpass, sizeof(askpass)) < 0){
				perror("write");
				exit(0);
			}
			//get chatroom password
			while(get_opcode(password) != 3)	{
				if(read(public_user->clientfd, password, sizeof(password)) < 0)	{
					perror("read");
					exit(0);
				}
			}
			//if action canceled by user, return
			if(!strcmp(password, "\n")){  
				printf("Join Action Canceled\n");
				return;
			}
			//if password not the same, reject
			if(strcmp(password+4, chatrooms[i].password)){ 
				if(write(public_user->displayfd , ack3, sizeof(ack3)) < 0){
					perror("write");
					exit(0);
				}
				return;
			}
			//add user to chat room
			memcpy(chatrooms[i].users[open_index].username, public_user->username, sizeof(chatrooms[i].users[open_index].username));
			chatrooms[i].users[open_index].set = 1;
			chatrooms[i].users[open_index].clientfd = public_user->clientfd;
			chatrooms[i].users[open_index].displayfd = public_user->displayfd;
			// notify user of success
			snprintf(ack1, sizeof(ack1), "User moved to %s", name+4);
			if(write(public_user->displayfd, ack1, sizeof(ack1)) < 0){
				perror("write");
				return;
			}
			//notify other users
			notify_user_enter_chat(public_user->clientfd, chatrooms[chatroom_index]);
			//remove user from public users
			bzero(public_user, sizeof(struct user));
		
			return;
		}
	}
	
	// notify failure
	if(write(public_user->displayfd, ack2, sizeof(ack2)) < 0){
		perror("write");
		exit(0);
	}
} //end joinChatRoom

//finds which chatroom the user corresponding to clientfd is located
//returns -1 if user not found in chatroom
int get_user_chatroom(int clientfd, struct chatroom *chatrooms)	{
	int index = -1;
	int i, j;

	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		for(j = 0; j < MAX_NUM_CHATS; ++j)	{
			if((chatrooms[j].users[i].clientfd == clientfd) && (chatrooms[j].set != 0))	{
				index = j;
				break;
			}
		}
	}

	return index;
}

int send_formatted_message(int clientfd, int displayfd, char *recvBuf, struct user *public_users)	{
	char message[4096];
	char data[512];
	time_t ticks;
	int k;

	//get system time
	if((ticks = time(NULL)) < 0)	{
		perror("time");
		exit(0);
	}	
	bzero(&message, sizeof(message));
	get_data(recvBuf, sizeof(recvBuf), data);
	//get user name
	for(k = 0; k < MAX_NUM_USERS; ++k)	{
		if(public_users[k].clientfd == clientfd)	{
			snprintf(message, sizeof(message), "[%.8s]%s: %s", ctime(&ticks)+11, public_users[k].username, data);
			break;
		}
	}
	//send message
	if(write(displayfd, message, sizeof(message)) < 0)	{
		perror("write");
		exit(0);
	}
}

int send_formatted_message_chat(int clientfd, int displayfd, char *recvBuf, struct chatroom chatroom)	{
	char message[4096];
	char data[512];
	time_t ticks;
	int k;

	//get system time
	if((ticks = time(NULL)) < 0)	{
		perror("time");
		exit(0);
	}	
	bzero(&message, sizeof(message));
	get_data(recvBuf, sizeof(recvBuf), data);
	//get user name
	for(k = 0; k < MAX_NUM_USERS; ++k)	{
		if(chatroom.users[k].clientfd == clientfd)	{
			//format message
			snprintf(message, sizeof(message), "[%.8s]%s: %s", ctime(&ticks)+11, chatroom.users[k].username, data);
			break;
		}
	}
	//send message
	if(write(displayfd, message, sizeof(message)) < 0)	{
		perror("write");
		exit(0);
	}
}

int send_pmsg(char *recvBuf, int size, int clientfd, struct user *public_users)	{
	char message[4096];
	char data[512];
	char name[64];
	char curr;
	time_t ticks;
	int found = -1;
	int i, j, k;
	int spaces_found = 0;
	//get system time
	if((ticks = time(NULL)) < 0)	{
		perror("time");
		exit(0);
	}	
	bzero(&message, sizeof(message));
	bzero(&name, sizeof(name));
	bzero(&data, sizeof(data));
	//get user name and message
	j = 0;
	k = 0;
	for(i = 4; i < size; ++i)	{
		curr = recvBuf[i];
		if((curr == ' ') && (spaces_found < 2))	
			++spaces_found;
		else if(curr == 0)
			break;
		else if(spaces_found == 1)	{
			name[j] = curr;
			++j;
		}
		else if(spaces_found > 1)	{
			data[k] = curr;
			++k;
		}
	}
	//get user name
	for(k = 0; k < MAX_NUM_USERS; ++k)	{
		if(public_users[k].clientfd == clientfd)	{
			//format message
			snprintf(message, sizeof(message), "[%.8s]private message from %s: %s", ctime(&ticks)+11, public_users[k].username, data);
			break;
		}
	}
	//check if user is a public user
	for(k = 0; k < MAX_NUM_USERS; ++k)	{
		if(!strcmp(public_users[k].username, name) && (public_users[k].set != 0))	{
			if(write(public_users[k].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
				return -1;
			}
			found = 0;
			break;
		}
	}
	//if nothing found, return error
	if(found == -1)
		return found;
	for(j = 0; j < MAX_NUM_USERS; ++j)	{
		if(public_users[j].clientfd == clientfd)	{
			//format and display message to sender
			bzero(&message, sizeof(message));
			snprintf(message, sizeof(message), "[%.8s]pmsg from %s to %s: %s", ctime(&ticks)+11, public_users[j].username, public_users[k].username, data);
			if(write(public_users[j].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
				return -1;
			}
			break;
		}
	}
	return found;
}

int send_pmsg_chat(char *recvBuf, int size, int clientfd, struct chatroom *chatrooms)	{
	char message[4096];
	char data[512];
	char name[64];
	char curr;
	time_t ticks;
	int found = -1;
	int i, j, k;
	int spaces_found = 0;
	int chatroom_index;
	//get system time
	if((ticks = time(NULL)) < 0)	{
		perror("time");
		exit(0);
	}	
	bzero(&message, sizeof(message));
	bzero(&name, sizeof(name));
	bzero(&data, sizeof(data));
	//get user name and message
	j = 0;
	k = 0;
	for(i = 4; i < size; ++i)	{
		curr = recvBuf[i];
		if((curr == ' ') && (spaces_found < 2))	
			++spaces_found;
		else if(curr == 0)
			break;
		else if(spaces_found == 1)	{
			name[j] = curr;
			++j;
		}
		else if(spaces_found > 1)	{
			data[k] = curr;
			++k;
		}
	}
	//get user name
	for(i = 0; i < MAX_NUM_CHATS; ++i)	{
		for(j = 0; j < MAX_NUM_USERS; ++j)	{
			if(chatrooms[i].users[j].clientfd == clientfd)	{
				chatroom_index = i;
				//format message
				snprintf(message, sizeof(message), "[%.8s]private message from %s: %s", ctime(&ticks)+11, chatrooms[i].users[j].username, data);
				break;
			}
		}
	}	
	//check if user is a private chat user
	for(k = 0; k < MAX_NUM_USERS; ++k)	{
		if(!strcmp(chatrooms[chatroom_index].users[k].username, name) && (chatrooms[chatroom_index].users[k].set != 0))	{
			if(write(chatrooms[chatroom_index].users[k].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
				return -1;
			}
			found = 0;
			break;
		}
	}
	//if nothing found, return error
	if(found == -1)
		return found;
	for(j = 0; j < MAX_NUM_USERS; ++j)	{
		if(chatrooms[chatroom_index].users[j].clientfd == clientfd)	{
			//format and display message to sender
			bzero(&message, sizeof(message));
			snprintf(message, sizeof(message), "[%.8s]pmsg from %s to %s: %s", ctime(&ticks)+11, chatrooms[chatroom_index].users[j].username, chatrooms[chatroom_index].users[k].username, data);
			if(write(chatrooms[chatroom_index].users[j].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
				return -1;
			}
			break;
		}
	}
	return found;
}

int notify_user_exit(int clientfd, struct user *public_users)	{
	int i;
	int found = -1;
	char message[128];

	//get username
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if(public_users[i].clientfd == clientfd)	{
			//format message
			snprintf(message, sizeof(message), "%s has exited\n", public_users[i].username);
			found = 0;
			break;
		}
	}
	//if not found, return error
	if(found == -1)
		return found;
	//display exit notification to all other users
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if((public_users[i].clientfd != clientfd) && (public_users[i].set !=0))	{
			if(write(public_users[i].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
			}
		}
	}

	return found;
}

int notify_user_exit_chat(int clientfd, struct chatroom chatroom)	{
	int i;
	int found = -1;
	char message[128];

	//get username
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if(chatroom.users[i].clientfd == clientfd)	{
			//format message
			snprintf(message, sizeof(message), "%s has exited\n", chatroom.users[i].username);
			found = 0;
			break;
		}
	}
	//if not found, return error
	if(found == -1)
		return found;
	//display exit notification to all other users
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if((chatroom.users[i].clientfd != clientfd) && (chatroom.users[i].set !=0))	{
			if(write(chatroom.users[i].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
			}
		}
	}

	return found;
}

int notify_user_enter_chat(int clientfd, struct chatroom chatroom)	{
	int i;
	int found = -1;
	char message[128];

	//get username
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if(chatroom.users[i].clientfd == clientfd)	{
			//format message
			snprintf(message, sizeof(message), "%s has entered\n", chatroom.users[i].username);
			found = 0;
			break;
		}
	}
	//if not found, return error
	if(found == -1)
		return found;
	//display exit notification to all other users
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if((chatroom.users[i].clientfd != clientfd) && (chatroom.users[i].set !=0))	{
			if(write(chatroom.users[i].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
			}
		}
	}

	return found;
}

void leaveChat(int clientfd, struct user *public_users){
	int q=-1;
	int w=-1;
	int e=-1;
	int chatroomppl;
	int i;
	int j;
	char message[256];

	for(i = 0; i < MAX_NUM_CHATS; ++i){
		chatroomppl=0;
		for(j =0; j < MAX_NUM_USERS; ++j){
			chatroomppl+=chatrooms[i].users[j].set;
			if(chatrooms[i].set!=0 && chatrooms[i].users[j].set!=0 && chatrooms[i].users[j].clientfd ==clientfd)
			{
				q=i; //which chat room they are in
				w=j; // which user index they are in withing that chat room
			}
		}
		if(q!=-1)
			break;
	}
	for(j =0; j < MAX_NUM_USERS; ++j){
		if(public_users[j].set==0)
		{
			e=j; //first empty stop within public_users.
			break;
		}
	}
	
	if(q==-1){ //user not found. 
		exit(0);

	}
	memcpy((char *)&public_users[e], (char *)&chatrooms[q].users[w],sizeof(struct user));
	//chatrooms[q].users[w].set=0;
	//remove user from chat
	bzero((char *)&chatrooms[q].users[w], sizeof(chatrooms[q].users[w]));

	chatroomppl--;
	//if user is last user within this chatroom, delete the chatroom. update chatroom array
	if(!chatroomppl)
	{
		chatrooms[q].set=0;
	}
	//notify other users
	bzero(&message, sizeof(message));
	snprintf(message, sizeof(message), "%s has left the chat\n", public_users[e].username); 
	for(i = 0; i < MAX_NUM_USERS; ++i)	{
		if(chatrooms[q].users[i].set)	{
			if(write(chatrooms[q].users[i].displayfd, message, sizeof(message)) < 0)	{
				perror("write");
				return;
			} 
		}
	}	
	//notify user
	if(write(public_users[e].displayfd, "now in lobby\n", sizeof("now in lobby\n")) < 0)	{
		perror("write");
		return;
	} 
}

int print_address()	{
	struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for(ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			if(!strcmp(ifa->ifa_name, "lo"))
				continue;
            printf("%s IP: %s\n", ifa->ifa_name, addressBuffer); 
		}
    }
    if(ifAddrStruct!=NULL) 
		freeifaddrs(ifAddrStruct);
    return 0;
}

int print_help_info(int displayfd)	{
	char message[4096] = "Commands:\n\t--create\t\tcreate a private chat room\n\t--pmsg <user name>\t\tsend a private message to a user\n\t--exit\t\texit the public chatroom\n\t--join\t\tjoin an existing chatroom\n\t--ls\t\tlist users\n\t--lsc\t\tlist chatrooms\n\t--lobby\t\tmove from privat chat to lobby\n\t--help\t\tdisplay this information\n";

	if(write(displayfd, message, sizeof(message)) < 0)	{
		perror("write");
		return -1;
	}

	return 0;
}
