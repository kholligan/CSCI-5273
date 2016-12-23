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
#include <errno.h>
#include <sys/select.h>

#define MAXBUFSIZE 100
#define ACKBUFSIZE 10
#define MESSAGEBUFSIZE 1500

/* You will have to modify the program below */

int main (int argc, char * argv[])
{

	int nbytes;                             // number of bytes send by sendto()
	int sock;                               // this will be our socket
	char buffer[MAXBUFSIZE];				// buffer/packets we will send/receive
	char ackMessage[ACKBUFSIZE] = "ACK";	// ACK that packet was received
	struct sockaddr_in remote;              //"Internet socket address structure"

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	sock = socket(remote.sin_family, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		printf("unable to create socket");
		exit(1);
	}

	const char delimiters[8] = " \n\r\0"; 		//characters we are eliminating to tokenize string
	char returnMessage[MESSAGEBUFSIZE];			//for passing strings between client/server
	char user_input[MAXBUFSIZE];				//parsing user input
	char fileBuffer[MAXBUFSIZE];				//reading the file into a buffer
	long fileSize;								//store total size of file to be passed to receiver
	FILE *fp;									//file pointer for file manipulation
	int n, sockn;
	fd_set readfds;								//set of filedescriptors for reading
	struct timeval timeout;						//timeout used in select()

	// Blocks till bytes are received
	struct sockaddr_in from_addr;
	socklen_t addr_length = sizeof(struct sockaddr);
	bzero(buffer,sizeof(buffer));



	// Set the timeout value 
	timeout.tv_sec = 3;
	timeout.tv_usec = 500000;

	while(1)
	{
		//clear the user input buffer
		memset(&user_input, 0, sizeof(user_input));
		printf("Enter command to send to server:");
		fgets(user_input,MAXBUFSIZE,stdin);
		//Tokenize the string looking for the first keyword ('get', 'put', etc.)
		char *token = strtok(user_input, delimiters);
		//Tokenize the remainder of the string ([FILENAME] or nothing)
		char *fileName = strtok(NULL, delimiters);

		// Clear the set of file descriptors
		FD_ZERO(&readfds);

		// Add socket to the set
		FD_SET(sock, &readfds);
		sockn = sock + 1;


		//Request to get a file
		if(strcmp(token, "get") == 0)
		{
			printf("Request to get file: %s\n", fileName);

			fp = fopen(fileName, "w+b");
			if(fp != NULL)
			{
				//untokenize message and send to server
				n = sprintf(user_input, "%s %s", token, fileName);
				n = sendto(sock, user_input, MAXBUFSIZE, 0, (struct sockaddr*)&remote, addr_length);

				int totalWritten = 0;
				fileSize = 0;

				//Receive the size of the file we are getting
				nbytes = recvfrom(sock, &fileSize, sizeof(fileSize),0, (struct sockaddr*)&from_addr, &addr_length);
				
				printf("Ready to receive %ld bytes\n", fileSize);
				//Loop until entire file is received
				while(totalWritten < fileSize)
				{
					//Identify when there is data in the socket ready to be read
					//Returns -1 on error, 0 on timeout, socket descriptor otherwise
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
						//Receive file data from the server, send ACK back
						nbytes = recvfrom(sock, fileBuffer, MAXBUFSIZE,0, (struct sockaddr*)&from_addr, &addr_length);
						n = sendto(sock, ackMessage, ACKBUFSIZE, 0, (struct sockaddr*)&remote, addr_length);
					}
					//Write to file
					n = fwrite(fileBuffer, MAXBUFSIZE, 1, fp);
					totalWritten += MAXBUFSIZE;
					printf("Total written: %d\n", totalWritten);
					//Clear the buffer
					//Prevents trailing duplicate data
					memset(&fileBuffer, 0, MAXBUFSIZE);					
				}

				printf("%d bytes written to file %s\n", totalWritten, fileName);

				fclose(fp);
			}
			else
			{
				fprintf(stderr, "Unable to open file: %s\n", fileName);
			}

		}
		//Put request
		else if (strcmp(token, "put") == 0)
		{
			printf("Request to put file: %s\n", fileName);
			fp = fopen(fileName, "r+b");
			if(fp != NULL)
			{
				//Rebuild the user input and send to server
				n = sprintf(user_input, "%s %s", token, fileName);
				n = sendto(sock, user_input, MAXBUFSIZE, 0, (struct sockaddr*)&remote, addr_length);

				//Read the size of the file and then return the pointer to beginning of file
				fseek(fp, 0, SEEK_END);
				fileSize = ftell(fp);
				rewind(fp);

				//Send the size of the file to the server
				n = sendto(sock, &fileSize, sizeof(fileSize), 0, 
					(struct sockaddr*)&remote, addr_length);
				printf("Sent file size (%ld bytes) to server\n", fileSize);

				int totalRead = 0;
				int bytesLeft = fileSize;
				int attempts = 0;
				//Loop until whole file is sent
				while(totalRead < fileSize)
				{
					fread(fileBuffer, MAXBUFSIZE, 1, fp);
					n = sendto(sock, fileBuffer, MAXBUFSIZE, 0,
						(struct sockaddr*)&from_addr, addr_length);
					if(n == -1)
					{
						printf("Error sending file: %s\n", strerror(errno));
						break;
					}

					//Identify when there is data in the socket ready to be read
					//Returns -1 on error, 0 on timeout, socket descriptor otherwise
					int rv = select(sockn, &readfds, NULL, NULL, &timeout);
					if(rv == -1)
					{
						printf("Error occurred in select()\n");
						break;
					}
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
						recvfrom(sock, ackMessage, ACKBUFSIZE,0, (struct sockaddr*)&from_addr, &addr_length);
					}
					totalRead += n;
					bytesLeft -= n; 
					printf("Sent %d bytes, %d bytes remaining. Running total: %d\n", n, bytesLeft, totalRead);
					
					//Flush the buffer to prevent sending duplicate trailing data
					//Useful for when data read < MAXBUFSIZE
					memset(&fileBuffer, 0, MAXBUFSIZE);				
				}

				fclose(fp);
				memset(&fileBuffer, 0, MAXBUFSIZE); //Clear the buffer when the whole thing is done
			}
			else
			{
				fprintf(stderr, "Unable to open file: %s\n", fileName);
			}
		}
		else if (strcmp(token, "ls") == 0)
		{
			//Send request to server
			n = sprintf(user_input, "%s %s", token, fileName);
			n = sendto(sock, user_input, MAXBUFSIZE, 0, (struct sockaddr*)&remote, addr_length);

			//Receive and print the file contents
			nbytes = recvfrom(sock, returnMessage, MESSAGEBUFSIZE,0, (struct sockaddr*)&from_addr, &addr_length);
			printf("%s",returnMessage);
		}
		//Send message to server and close the socket
		else if (strcmp(token, "exit") == 0)
		{
			n = sprintf(user_input, "%s %s", token, fileName);
			n = sendto(sock, user_input, MAXBUFSIZE, 0, (struct sockaddr*)&remote, addr_length);

			close(sock);
			exit(1);
		}
		else
		{
			printf("USAGE:  get [FILENAME], put [FILENAME], ls, exit\n");

		}
	}

	return 0;
}

