uint16_t getOpcode(char *recvBuf)	{
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
	if((op == 1) || (op == 2) || (op == 3) || (op == 4) || (op == 5))	{
		return op;
	}
	else	{
		return 0;
	}	
}


//parses the packet 
void parseAndPrintRWPacket(char *recvBuf, char *filename, char *mode, int nrecv, struct sockaddr_in clientAddr, int op)	{
	int i;
    int j = 0;
    int k = 0;
	int foundFileEnd = 0;
    char ipaddr[512];

	//parse the packet
    for(i = 2; i < nrecv; ++i)  {
		if(!foundFileEnd)  {
            filename[j] = recvBuf[i];
            if(filename[j] == 0)
                foundFileEnd = 1;
            else
                ++j;
        }
        else    {
            mode[k] = recvBuf[i];
            if(mode[k] == 0)
                break;
            else
                ++k;
        }
    }
    //print request type
    switch(op)  {
        case RRQ:
            printf("RRQ ");
            break;
        case WRQ:
            printf("WRQ ");
            break;
        case DATA:
            printf("DATA ");
            break;
        case ACK:
            printf("ACK ");
            break;
        case ERROR:
            printf("ERROR ");
            break;
        default:
            printf("Error: invalid opcode\n");  
			exit(0);
    }
    //printf filename
    printf("%s ", filename);
    //printf mode
    printf("%s ", mode);
    //convert ip address
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ipaddr, INET_ADDRSTRLEN);
    //print ip address
    printf("%s:", ipaddr);
    //print port number
    printf("%d\n", clientAddr.sin_port);
}


uint16_t parseAckPacket(char *recvBuf, int nrecv)	{
	int i;
	int j = 0;
	char block[2];
	uint16_t blocknum = 0;
	if(nrecv < 4)	{
		perror("packet too small");
		return 0;
	}
	for(i = 2; i < nrecv; ++i)	{
		block[j] = recvBuf[i];
		++j;
		if(j > 1)
			break;
	}
	//convert block number from char array to uint16_t
    blocknum = blocknum | (block[0] << 8);
    blocknum = blocknum | block[1];
	
	return blocknum;
}


int sendErrorPacket(int sockfd, struct sockaddr_in clientAddr, int clientAddrSize, int errorcode, char *errormsg)   {
    //populate error packet
    struct errorPacket error;
	bzero(&error, sizeof(error));
    error.opcode = htons(ERROR);	// ERROR == Error opcode
    error.errorcode = htons(errorcode);      // 1 == File not found error code
    strncpy(error.errmsg, errormsg, strlen(errormsg));
	//strcpy(error.errmsg, errormsg);  

    //send error packet to client
    if(sendto(sockfd, (char*)&error, 512, 0, (struct sockaddr*) &clientAddr, clientAddrSize) < 0)	{
        return 0;
    }
    return 1;
}

