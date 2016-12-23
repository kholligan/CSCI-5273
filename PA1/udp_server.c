#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <dirent.h>

#define MAXBUFSIZE 100
#define ACKBUFSIZE 10
#define MESSAGEBUFSIZE 1500

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	socklen_t remote_length;         	//length of the sockaddr_in structure
	int nbytes;                        	//number of bytes we receive in our message
	char buffer[MAXBUFSIZE];            //a buffer to store our received message
	char ackMessage[ACKBUFSIZE] = "ACK"; //ACK message to indicate receipt


	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type UDP (datagram)
	sock = socket(sin.sin_family, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		printf("Error in socket(): %s\n", strerror(errno));
		exit(1);
	}

	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("Error in bind(): %s\n", strerror(errno));
		exit(1);
	}

	remote_length = sizeof(remote); 	//size of sockaddr_in

	
	const char delimiters[8] = " \0\n\r";		//Characters to use for tokenizing
	char returnMessage[MESSAGEBUFSIZE];			//Larger buffer for message passing
	char fileBuffer[MAXBUFSIZE];				//Buffer for sending file packets
	FILE *fp;									//File pointer for reading/writing
	DIR *dirp;									//Directory pointer used for ls
	int n, sockn;								
	fd_set readfds;								//Set of file descriptors for socket()
	struct timeval timeout;						//Timeout used for socket()

	// Set the timeout value 
	timeout.tv_sec = 3;
	timeout.tv_usec = 500000;

	while(1)
	{
		//waits for an incoming message
		bzero(buffer,sizeof(buffer));
		printf("Waiting to receive message...\n");
		//Receive the string containing user input
		nbytes = recvfrom(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr*) &remote, &remote_length);
		//Tokenize the user input
		char *token = strtok(buffer, delimiters);
		char *fileName = strtok(NULL, "\n\r");

		// Clear the set
		FD_ZERO(&readfds);

		// Add socket to the set
		FD_SET(sock, &readfds);
		sockn = sock + 1;

		//Get request
		//Send the specified filename to the client
		if(strcmp(token, "get") == 0)
		{
			printf("Received get request for file: %s\n", fileName);

			//Open in read/binary mode
			fp = fopen(fileName, "r+b");
			if(fp != NULL)
			{
				//Determine the size of the file, then reset fp to beginning of file
				fseek(fp, 0, SEEK_END);
				long fileSize = ftell(fp);
				rewind(fp);

				//Send file size to the client
				n = sendto(sock, &fileSize, sizeof(fileSize), 0, 
					(struct sockaddr*)&remote, remote_length);
				printf("Sent file size (%ld bytes) to client\n", fileSize);

				int totalRead = 0;
				int bytesLeft = fileSize;
				int attempts = 0;
				//Loop until entire file sent
				while(totalRead < fileSize)
				{
					//read in the file in blocks of MAXBUFSIZE, then send to client
					fread(fileBuffer, MAXBUFSIZE, 1, fp);
					n = sendto(sock, fileBuffer, MAXBUFSIZE, 0,
						(struct sockaddr*)&remote, remote_length);
					if(n == -1)
					{
						printf("Error sending file: %s\n", strerror(errno));
						break;
					}
					//Wait for an ACK or timeout and send again
					int rv = select(sockn, &readfds, NULL, NULL, &timeout);
					if(rv == -1)
					{
						printf("Error occurred in select()\n");
						break;
					}
					//If timed out, send the buffer again
					else if (rv == 0)
					{
						if(attempts < 2)
						{
							attempts++;
							printf("Timeout occurred. Resending packet.\n");
							//Rewind the fp so that it reads in the same packet it just tried to send
							fseek(fp, -MAXBUFSIZE, SEEK_CUR);
							continue;
						}
						else
						{
							printf("Could not send packet after %d attempts. Ending...\n", attempts);
							break;
						}
					}
					else
					{
						// file descriptor has data
						// Read in the ACK
						recvfrom(sock, ackMessage, ACKBUFSIZE,0, (struct sockaddr*)&remote, &remote_length);
					}
					totalRead += n;
					bytesLeft -= n; 
					printf("Sent %d bytes, %d bytes remaining. Running total: %d\n", n, bytesLeft, totalRead);
					//Clear the file buffer
					//Useful for when data sent is less than MAXBUFSIZE (i.e. last packet)
					memset(&fileBuffer, 0, MAXBUFSIZE);				
				}
				fclose(fp);
				memset(&fileBuffer, 0, MAXBUFSIZE);	
			}
			else
			{
				//Send the error to the client
				fprintf(stderr, "Unable to open file: %s\n", fileName);
				nbytes = sendto(sock, stderr, MAXBUFSIZE, 0, 
					(struct sockaddr*)&remote, remote_length);
			}
		}

		//Get a file from the client
		else if (strcmp(token, "put") == 0)
		{

			printf("Received put request for file: %s\n", fileName);

			fp = fopen(fileName, "w+b");
			if(fp != NULL)
			{
				int totalWritten = 0;
				long fileSize = 0;

				//Receive the size of the file 
				nbytes = recvfrom(sock, &fileSize, sizeof(fileSize),0, (struct sockaddr*)&remote, &remote_length);
				
				printf("Ready to receive %ld bytes\n", fileSize);
				while(totalWritten < fileSize)
				{
					//Wait to receive the packet and send in ack or timeout
					int rv = select(sockn, &readfds, NULL, NULL, &timeout);
					if (rv == -1)
					{
						printf("Error in select()\n");
						break;
					}
					else if(rv == 0)
					{
						printf("Timeout occurred!");
						continue;
					}
					else
					{
						//Receive packet, then send ACK to client
						nbytes = recvfrom(sock, fileBuffer, MAXBUFSIZE,0, (struct sockaddr*)&remote, &remote_length);
						n = sendto(sock, ackMessage, ACKBUFSIZE, 0, (struct sockaddr*)&remote, remote_length);
					}
					n = fwrite(fileBuffer, MAXBUFSIZE, 1, fp);
					totalWritten += MAXBUFSIZE;
					printf("Total written: %d\n", totalWritten);
					memset(&fileBuffer, 0, MAXBUFSIZE);
				}

				printf("%d bytes written to file %s\n", totalWritten, fileName);

				fclose(fp);
			}
			else
			{
				fprintf(stderr, "Unable to open file: %s\n", fileName);
				nbytes = sendto(sock, stderr, MAXBUFSIZE, 0, 
					(struct sockaddr*)&remote, remote_length);
			}
		}
		//Return list of directories 
		else if (strcmp(token, "ls") == 0)
		{
			struct dirent *dp;
			int length = 0;

			//Open the current directory
			dirp = opendir(".");
			if(dirp != NULL)
			{
				//readdir will return NULL when reaching end of the directory
				while((dp=readdir(dirp)) != NULL)
				{
					//Append the list of directory entries to the buffer
					length += sprintf(returnMessage + length,"%s\n", dp->d_name);
				}

				//Send the list of directories to the client
				n = sendto(sock, returnMessage, MESSAGEBUFSIZE, 0,
					(struct sockaddr*)&remote, remote_length);
				if(n == -1)
				{
						printf("Error sending ls: %s\n", strerror(errno));
						break;
				}

				//Close the directory and clean up
				closedir(dirp); 
			}
			else
			{
				fprintf(stderr, "Unable to open directory.\n");
			}

			memset(&returnMessage, 0, MESSAGEBUFSIZE);
		}
		else if (strcmp(token, "exit") == 0)
		{
			close(sock);
			exit(1);
		}
		else
		{
			printf("Unknown request\n Usage: get [FILENAME], put [FILENAME], ls, exit\n");
		}
	}

	return 0;
}

