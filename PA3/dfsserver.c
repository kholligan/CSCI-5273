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
#include <sys/stat.h>

#define MAXBUFSIZE 1024
#define MAXUSERPWSIZE 30
#define MAXNUMUSERS 10
#define MAXFILES 25

struct config
{
	char username[MAXNUMUSERS][MAXUSERPWSIZE];
	char password[MAXNUMUSERS][MAXUSERPWSIZE];
};

struct files
{
    int count;
    char fileName[MAXFILES][MAXBUFSIZE];
    int partA;
    int partB;
};

//Read a configuration file. 
//File has to have same leading token (Listen, DocumentRoot, etc.) as ws.conf 
int readConfigFile(char *fileName, struct config *confData)
{
    FILE *fp;
    char fileBuffer[MAXBUFSIZE];
    char tempUser[MAXUSERPWSIZE];
    char tempPass[MAXUSERPWSIZE];
    // char tempBuffer[MAXBUFSIZE];
    // const char delimiters[4] = " \n";
    int i = 0;

    fp = fopen(fileName, "r");
    //If successfully opened file
    if(fp != NULL)
    {
    	//Read entire file line by line
        while(fgets(fileBuffer, MAXBUFSIZE, fp) != NULL)
        {
        	memset(&tempUser, 0, MAXUSERPWSIZE);
        	memset(&tempPass, 0, MAXUSERPWSIZE);
            //If the line is a commment, do nothing
            if (strncmp(fileBuffer, "#",1) == 0 )
            {
                continue;
            }

            //Scan username and pw into config struct
        	sscanf(fileBuffer, "%s %s", tempUser, tempPass);
        	strcpy(confData->username[i], tempUser);
        	strcpy(confData->password[i], tempPass);
        	i++;

        	//Stop reading the file if we've reached config array limit
        	if(i == MAXNUMUSERS)
        	{
        		break;
        	}
        }
    }
    //return -1 on error
    else
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 1;
}

void list(int sock, struct files *fileList)
{
    int max = fileList->count;
    //Tell the client the number of files
    send(sock, &max, sizeof(max), 0);
    if(fileList->count != 0)
    {
        for(int i =0; i<max; i++)
        {
            //send the name of the file
            send(sock, fileList->fileName[i], MAXBUFSIZE, 0);
            //Send the parts that the server has
            send(sock, &fileList->partA, sizeof(fileList->partA), 0);
            send(sock, &fileList->partB, sizeof(fileList->partB), 0);
        }
    }
}

void get(int sock, char* path, char* fileName, int filePart)
{
	FILE *fp;

	char returnMessage[MAXBUFSIZE];
	char serverPath[MAXBUFSIZE];
	char fileBuffer[MAXBUFSIZE];
    char c;
	int n;
	snprintf(serverPath, MAXBUFSIZE, "./%s.%s.%d", path, fileName, filePart);

	fp = fopen(serverPath, "r+b");
	if(fp != NULL)
	{
		//Determine the size of the file, then reset fp to beginning of file
		fseek(fp, 0, SEEK_END);
		long fileSize = ftell(fp);
		rewind(fp);

		//Tell client how much to receive
		send(sock, &fileSize, sizeof(fileSize), 0);

		int totalRead = 0;

		while(totalRead < fileSize)
		{
			//read in the file in blocks of MAXBUFSIZE, then send to client
            c = fgetc(fp);
			n = send(sock, &c, sizeof(c), 0);
			if(n == -1)
			{
				printf("Error sending file: %s\n", strerror(errno));
				break;
			}

			totalRead += n;

			//Clear the file buffer
			//Useful for when data sent is less than MAXBUFSIZE (i.e. last packet)
			memset(&fileBuffer, 0, MAXBUFSIZE);	
		}

		//Close the file pointer and clear the buffer
		fclose(fp);
		memset(&fileBuffer, 0, MAXBUFSIZE);

	}
	else
	{
		snprintf(returnMessage, MAXBUFSIZE, "Invalid file name. File does not exist.");
		send(sock, returnMessage, MAXBUFSIZE, 0);
	}

}

void put(int sock, char* path, char* fileName, int filePart, struct files *fileList)
{
	FILE *fp;
	char serverPath[MAXBUFSIZE];
	char fileBuffer[MAXBUFSIZE];
    char returnMessage[MAXBUFSIZE];

	int n;
    char c;

	snprintf(serverPath, MAXBUFSIZE, "./%s.%s.%d", path, fileName, filePart);

	fp = fopen(serverPath, "w+b");
	if(fp != NULL)
	{
		int totalWritten = 0;
		long fileSize = 0;

		//Receive the size of the file
		recv(sock, &fileSize, sizeof(fileSize), 0);
        // printf("File size: %d\n", fileSize);

		while(totalWritten < fileSize)
		{
			//Receive the data from clien and write to file)
            // printf("about to receive\n");
			n = recv(sock, &c, sizeof(c), 0);
            if(n < 0)
            {
                perror("error receiving");
            }
            // printf("received a char. n = %d\n", n);
			// fwrite(c, sizeof(c), 1, fp);
            fputc(c, fp);
			totalWritten += n;
			memset(&fileBuffer, 0, MAXBUFSIZE);
		}
        // printf("File written\n");
		fclose(fp);

        //Check if we already have this entry
        if(strcmp(fileList->fileName[fileList->count], fileName) == 0)
        {
            fileList->partB = filePart;
            fileList->count++;
        }
        else
        {
            strcpy(fileList->fileName[fileList->count], fileName);
            fileList->partA = filePart;
        }
        
	}
	else
	{
		snprintf(returnMessage, MAXBUFSIZE, "Could not write to file.");
		send(sock, returnMessage, MAXBUFSIZE, 0);
	}

}

int main(int argc , char *argv[])
{
    int socket_desc , client_sock , c , read_size;
    struct sockaddr_in server , client;
    
    struct config confData;
    struct files fileList;
    fileList.count = 0;
    int status, port;
    int errnum = 0;
    int length = 0;
    char *fileName;
    char path[MAXBUFSIZE];
    pid_t pid;

    //Config file
    fileName = "./dfs.conf";

    //Make sure user enters valid number of args
    if(argc < 3)
    {
        printf("USAGE: [FOLDER] [PORT NUMBER]\n");
        return 1;
    }

    //Read config file into config struct
    status = readConfigFile(fileName, &confData);
    if(status == -1)
    {
        fprintf(stderr, "Error opening file %s: %s\n", fileName, strerror(errnum));
        return 1;
    }

    //Set the path and port from supplied command line entry
    length += snprintf(path, MAXBUFSIZE, "%s", argv[1]);
    port = atoi(argv[2]);

    //Create directory for users if none exists
    struct stat st = {};
    if(stat(path, &st) == -1)
    {
    	mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

	//Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }

    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
     
    //Listen
    if ( listen(socket_desc , 3) < 0 )
    {
        perror("Listen failed. Error");
        return 1;
    }
     
    //Accept and incoming connection
    c = sizeof(struct sockaddr_in);

    while(1)
    {
        char client_message[MAXBUFSIZE];
        char userCommand[MAXBUFSIZE];
        char userFile[MAXBUFSIZE];
        char userName[MAXBUFSIZE];
        char userPassword[MAXBUFSIZE];
        char filePart[MAXBUFSIZE];
        char returnMessage[MAXBUFSIZE];

        //accept connection from an incoming client
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }

        //Fork a child process to handle requests
        if ( (pid = fork()) == 0 )
        {

            memset(&userName, 0, MAXBUFSIZE);
            memset(&userPassword, 0, MAXBUFSIZE);
            //Child closes the listening socket
            close(socket_desc);

            //Do some stuff with client_sock
            int validUser = 0;

            //Compare username and password to conf
            //Make a folder for the user if one doesn't exist
            while(!validUser)
            {
                recv(client_sock, client_message, MAXBUFSIZE, 0);
                sscanf(client_message, "%s %s", userName, userPassword);
                for(int i=0; i<MAXNUMUSERS; i++)
                {
                    if( (strcmp(confData.username[i], userName) == 0) 
                        && (strcmp(confData.password[i], userPassword) == 0) )
                    {
                        validUser = 1;
                        struct stat st = {};
                        length += snprintf(path+length, MAXBUFSIZE-length, "/%s/", confData.username[i]);
                        //Check status of existing directory
                        if(stat(path, &st) == -1)
                        {
                            mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                        }
                        snprintf(returnMessage, MAXBUFSIZE, "Successfully authenticated.");
                        send(client_sock, returnMessage, MAXBUFSIZE, 0);
                        break; //stop comparing once we find a match
                    }
                }
                if(!validUser)
                {
                    snprintf(returnMessage, MAXBUFSIZE, "Invalid username or password.");
                    send(client_sock, returnMessage, MAXBUFSIZE, 0);
                }
                memset(&client_message, 0, MAXBUFSIZE);
            }
            //Receive a message from client
            while( (read_size = recv(client_sock , client_message , MAXBUFSIZE , 0)) > 0 )
            {
                //clear the buffers
                memset(&userCommand, 0, MAXBUFSIZE);
                memset(&userFile, 0, MAXBUFSIZE);
                memset(&filePart, 0, MAXBUFSIZE);

                int filePartNum = 0;


                //read in the client request in format "[COMMAND] [/PATH/FILE] [PART NUMBER]"
                sscanf(client_message, "%s %s %s", userCommand, userFile, filePart);
                filePartNum = atoi(filePart);

            	char extractedPath[MAXBUFSIZE];
            	char extractedFile[MAXBUFSIZE];
            	//Extract path and fileName into two parts
            	//Find the last occurence in the string
				const char *dot = strrchr(userFile, '/');
				if(dot == NULL)
				{
					// strcpy(dot, "\n");
				}
				char* token = strtok(userFile, dot);
				snprintf(extractedPath, MAXBUFSIZE, "%s", token);
				snprintf(extractedFile, MAXBUFSIZE, "%s", dot+1);

            	//Test against user commands and process requests
            	if( (strcmp(userCommand, "LIST") == 0) || (strcmp(userCommand, "list") == 0) )
            	{
            		list(client_sock, &fileList);
            	}
            	if( (strcmp(userCommand, "GET") == 0) || (strcmp(userCommand, "get") == 0) )
            	{
            		get(client_sock, path, extractedFile, filePartNum);
            	}
            	if( (strcmp(userCommand, "PUT") == 0) || (strcmp(userCommand, "put") == 0) )
            	{
            		put(client_sock, path, extractedFile, filePartNum, &fileList);
            	}
                if( (strcmp(userCommand, "ping") == 0) )
                {
                    memset(&returnMessage, 0, MAXBUFSIZE);
                    snprintf(returnMessage, MAXBUFSIZE, "%s", "Still alive.");
                    send(client_sock, returnMessage, MAXBUFSIZE, 0);
                }
                memset(&client_message, 0, MAXBUFSIZE);
            }
             
            if(read_size == 0)
            {
                // printf("Client disconnected: %d\n", client_sock);
                fflush(stdout);
            }
            else if(read_size == -1)
            {
                perror("recv failed");
            }

            //Close sockets and terminate child process
            close(client_sock);
            exit(0);
        }

    }
    
    // Close the parent socket
    // printf("Parent disconnected: %d\n", socket_desc);
    close(socket_desc);

	return 0;
}
