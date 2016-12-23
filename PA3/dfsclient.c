// Distributed File System - Client
// Author: Kevin Holligan
// Network Systems

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
// #include <openssl/md5.h>

#define MAXBUFSIZE 1024
#define NUMSERVERS 4
#define SERVERNAME 50
#define LISTSIZE 4096
#define MAXFILES 25

struct config 
{
    char serverName[NUMSERVERS][SERVERNAME];
    char serverAddr[NUMSERVERS][SERVERNAME];
    int ports[NUMSERVERS];
    char username[SERVERNAME];
    char password[SERVERNAME];
};

struct files
{
    int count;
    char fileName[MAXFILES][MAXBUFSIZE];
    int parts[MAXFILES][4];
};
//For negative modulo division
//from http://stackoverflow.com/questions/4003232/how-to-code-a-modulo-operator-in-c-c-obj-c-that-handles-negative-numbers
int mod (int a, int b)
{
   if(b < 0) //you can check for b == 0 separately and do what you want
     return mod(a, -b);   
   int ret = a % b;
   if(ret < 0)
     ret+=b;
   return ret;
}

//Read a configuration file. 
//File has to have same leading token (Listen, DocumentRoot, etc.) as ws.conf 
int readConfigFile(char *fileName, struct config *confData)
{
    FILE *fp;
    char fileBuffer[MAXBUFSIZE];
    char tempBuffer[MAXBUFSIZE];
    const char delimiters[8] = " \n";
    int i=0;

    fp = fopen(fileName, "r");
    //If successfully opened file
    if(fp != NULL)
    {
        while(fgets(fileBuffer, MAXBUFSIZE, fp) != NULL)
        {
            //If the line is a commment, do nothing
            if (strncmp(fileBuffer, "#",1) == 0 )
            {
                continue;
            }
            //Preserve our original file buffer (strtok overwrites)
            strncpy(tempBuffer, fileBuffer, MAXBUFSIZE);
            //Read the indicator token
            char *token = strtok(tempBuffer, delimiters);
            //Tokenize the second portion of the string and store in config struct
            if(strcmp(token, "Server") == 0)
            {
                token = strtok(NULL, " :\n");
                strcpy(confData->serverName[i], token);
                token = strtok(NULL, " :\n");
                strcpy(confData->serverAddr[i], token);
                token = strtok(NULL, " :\n");
                //Convert port number to an int and store
                sscanf(token, "%d", &confData->ports[i]);
                i++;
            }
            else if(strcmp(token, "Username:") == 0)
            {
                token = strtok(NULL, "\n");
                strcpy(confData->username, token);
            }
            else
            {
                token = strtok(NULL, "\n");
                strcpy(confData->password, token);
            }
        }
    }
    //Print error and return -1
    else
    {
        printf("Unable to open config file. \n");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 1;
}

char encryptChar(char c, struct config *confData)
{
    char a;
    char b;
    int cipher=0;
    for(int i = 0; i<SERVERNAME; i++)
    {
        a = confData->username[i];
        b = confData->password[i];
        cipher = cipher + a + b;
    }

    c = (c + cipher) % 127; //ASCII table, no special characters

    return c;
}

char decryptChar(char c, struct config *confData)
{
    char a;
    char b;
    int num = c;
    int cipher=0;
    for(int i = 0; i<SERVERNAME; i++)
    {
        a = confData->username[i];
        b = confData->password[i];
        cipher = cipher + a + b;
    }

    num = (num - cipher);
    num = mod(num, 127); //ASCII table, no special characters

    return (char) num;
}

void helperFileReceiver(struct config *confData, int sock, char* message, FILE *fp)
{
    long fileSize=0;
    int totalWritten=0;
    char* fileBuffer[MAXBUFSIZE];
    char c;

    send(sock, message, MAXBUFSIZE, 0); //Send command to server
    recv(sock, &fileSize, sizeof(fileSize), 0); //Receive size of file

    while(totalWritten < fileSize)
    {
        int n = recv(sock, &c, sizeof(c), 0);
        if(n <= 0)
        {

        }
        c = decryptChar(c, confData);
        fputc(c, fp);

        totalWritten += n;
        memset(&fileBuffer, 0, MAXBUFSIZE);
    }
}

void helperFileSender(int sock, int j, long partSize, FILE *part1, FILE *part2, FILE *part3, FILE* part4, struct config *confData)
{
    char fileBuffer[MAXBUFSIZE];
    char c;

    int totalWritten = 0;

    //Set file pointers to beginning of file
    rewind(part1);
    rewind(part2);
    rewind(part3);
    rewind(part4);

    //Send file
    while(totalWritten < partSize)
    {
        if(j == 1)
        {   
            c = fgetc(part1);
        }
        else if(j == 2)
        {
            c = fgetc(part2);
        }
        else if(j == 3)
        {
            c = fgetc(part3);
        }
        else
        {
            c = fgetc(part4);
        }
        c = encryptChar(c, confData);
        int n = send(sock, &c, sizeof(c), 0);

        totalWritten += n;
        memset(&fileBuffer, 0, MAXBUFSIZE);
    }

}


//Adopted from http://www.askyb.com/cpp/openssl-md5-hashing-example-in-cpp/
//Cannot get openssl/md5 to run on my machine. Removing this section
int hashFile(char *fileName)
{
    // unsigned char digest[MD5_DIGEST_LENGTH];
    // unsigned char hash[MAXBUFSIZE];
    // unsigned long result = 0; 
    // unsigned long mod = 4;

    // MD5_CTX ctx;
    // MD5_Init(&ctx);
    // MD5_Update(&ctx, fileName, strlen(fileName));
    // MD5_Final(digest, &ctx);

    // for(int i = 0; i < MD5_DIGEST_LENGTH + 1; i++) {
    //     int curVal = (hash[i] > '9')?(hash[i] &~0x20)-'A'+10:(hash[i] - '0');
    //     result = result*16 + curVal;

    //     if (result >= mod) {
    //         result %= mod;
    //     }
    // }

    // return (int)result;
    char c = fileName[4];
    int mod = 4;
    int result = c%mod;
    return result;
}

void list(int sock[], struct files *fileList)
{

    char message[MAXBUFSIZE];
    char buf[MAXBUFSIZE];
    int recvPart1 = 0;
    int recvPart2 = 0;

    snprintf(message, MAXBUFSIZE, "LIST / /");

    //Clear any existing lists
    memset(&fileList->fileName, 0, sizeof(fileList->fileName));
    memset(&fileList->parts, 0, sizeof(fileList->parts));
    fileList->count = 0;

    for(int i=0; i<4; i++)
    {
        //check if server is offline
        if(sock[i] != -1)
        {
            //send message to server
            send(sock[i], message, MAXBUFSIZE, 0);
            //Retrieve number of files
            if(recv(sock[i], &fileList->count, sizeof(fileList->count), 0) <= 0)
            {
                continue;;
            }
            //Receive file names
            if(fileList->count != 0)
            {
                for (int j = 0; j<fileList->count; j++)
                {
                    //receive fileName;
                    recv(sock[i], buf, MAXBUFSIZE, 0);
                    //Check if we already have the file in client lists
                    if( (strcmp(fileList->fileName[j], buf) != 0) )
                    {
                        strcpy(fileList->fileName[j], buf);
                    }

                    //Receive the list of pieces the server has
                    recv(sock[i], &recvPart1, sizeof(recvPart1),0);
                    recv(sock[i], &recvPart2, sizeof(recvPart2),0);

                    //Fix indexing off-by-one
                    recvPart1--;
                    recvPart2--;

                    //Set part to true
                    fileList->parts[j][recvPart1] = 1;
                    fileList->parts[j][recvPart2] = 1;
                }
            }
        }
    }

    int completeFile = 0;
    //Print the list
    if(fileList->count == 0)
    {
        printf("Servers have no files\n");
    }
    else
    {

        for(int i=0; i<fileList->count; i++)
        {
            //Check if we have all four parts
            if(fileList->parts[i][0] == 1 && fileList->parts[i][1] == 1
                && fileList->parts[i][2] == 1 && fileList->parts[i][3] == 1)
            {
                completeFile = 1;
            }

            // Add "incomplete" to file name if we don't have enough parts
            if(!completeFile)
            {
                strcat(fileList->fileName[i], " [incomplete]");
            }

            strcat(fileList->fileName[i], "\n");
            printf("%d. %s", i+1, fileList->fileName[i]);
        }
    }

}

void get(int sock[], char* path, char* fileName, struct config *confData)
{
    FILE *fp;
    int hash;
    char message[MAXBUFSIZE];
    int offlineServerCount =0;
    int completeFile = 1;


    for(int i=0; i<4; i++)
    {
        if(sock[i] == -1) 
        {
            offlineServerCount++;  
        }
    }
    if(offlineServerCount>=3)
    {
        completeFile = 0;
    }
    hash = hashFile(fileName);
    if(completeFile)
    {  
        fp = fopen(fileName, "a+b"); 
        switch(hash)
        {
            //(1,2), (2,3), (3,4), (4,1)
            case 0:
                if(sock[0] != -1 && sock[2] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else if (sock[1] != -1 && sock[3] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else
                {
                    printf("File is incomplete, cannot be downloaded from server.\n");
                }
                break;
            //(4,1), (1,2), (2,3), (3,4)
            case 1:
                if(sock[0] != -1 && sock[2] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else if(sock[1] != -1 && sock[3] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else
                {
                    printf("File is incomplete, cannot be downloaded from server.\n");
                }
                break;
            //(3,4), (4,1), (1,2), (2,3)
            case 2:
                if(sock[1] != -1 && sock[3] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else if(sock[0] != -1 && sock[2] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else
                {
                    printf("File is incomplete, cannot be downloaded from server.\n");
                }
                break;
            //(2,3), (3,4), (4,1), (1,2)
            case 3:
                if(sock[0] != -1 && sock[2] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[0], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[2], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else if(sock[1] != -1 && sock[3] != -1)
                {
                    //Parts 1 and 2
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 1);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 2);
                    helperFileReceiver(confData, sock[3], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                    //Parts 3 and 4
                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 3);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);

                    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 4);
                    helperFileReceiver(confData, sock[1], message, fp);
                    memset(&message, 0, MAXBUFSIZE);
                }
                else
                {
                    printf("File is incomplete, cannot be downloaded from server.\n");
                }
                break;
        }
        fclose(fp);
    }
    else
    {
        printf("File is incomplete, cannot be downloaded from server.\n"); 
    }

}

void put(int sock[], char* path, char* fileName, struct config *confData)
{
    FILE *fp, *part1, *part2, *part3, *part4;
    long fileSize, partSize;
    int hash;
    char message[MAXBUFSIZE];
    int j =0;

    //Create files to be written to
    part1 = tmpfile();
    part2 = tmpfile();
    part3 = tmpfile();
    part4 = tmpfile();

    fp = fopen(fileName, "r+b");
    //Read the size of the file and then return the pointer to beginning of file
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    rewind(fp);

    partSize = fileSize/4;

    //Write the parts to the files
    char c;
    int charsWritten =0;
    while( (c = fgetc(fp)) != EOF )
    {
        if(charsWritten < partSize)
        {
            fputc(c, part1);
        }
        else if(charsWritten < (partSize * 2))
        {
            fputc(c, part2);
        }
        else if(charsWritten < (partSize * 3))
        {
            fputc(c, part3);
        }
        else
        {
            fputc(c, part4);
        }
        charsWritten++;
    }
    hash = hashFile(fileName);

    int offlineServerCount =0;
    for(int i=0; i<4; i++)
    {
        if(sock[i] == -1) 
        {
            offlineServerCount++;  
        }
    }
    if(offlineServerCount>=1)
    {
        printf("Too many servers offline to send file.\n");
        return;
    }
    //Figure which parts to send to which server
    switch(hash)
    {
        //(1,2), (2,3), (3,4), (4,1) 
        case 0:
            for(int i=0; i<4; i++)
            {
                j = i+1;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n = send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0)
                {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send first file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);

                j = i+2;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send the second file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);
            }
            break;
        //(4,1), (1,2), (2,3), (3,4)
        case 1:
            for(int i=0; i<4; i++)
            {
                j = i+4;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n = send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0)
                {
                    printf("Server offline down, trying next one. n: %d\n", n);
                    continue;
                }
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send first file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);

                j = i+1;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send the second file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);
            }
            break;
        //(3,4), (4,1), (1,2), (2,3)
        case 2:
            for(int i=0; i<4; i++)
            {
                j = i+3;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n= send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0)
                {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send first file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);

                j = i+4;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send the second file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);
            }
            break;
        //(2,3), (3,4), (4,1), (1,2)
        case 3:
            for(int i=0; i<4; i++)
            {
                j = i+2;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n =send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0)
                {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send first file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);

                j = i+3;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                send(sock[i], &partSize, sizeof(partSize), 0); //Send size of file

                //Send the second file
                helperFileSender(sock[i], j, partSize, part1, part2, part3, part4, confData);

                memset(&message, 0, MAXBUFSIZE);
            }
            break;
    }

    fclose(fp);
}

int main(int argc, char *argv[]) {
	// int socket_desc, client_sock , c , read_size;
    struct sockaddr_in client;
    
    struct config confData;
    memset(&confData.username, 0, sizeof(confData.username));
    memset(&confData.password, 0, sizeof(confData.password));

    struct files fileList;
    memset(&fileList.parts, 0, sizeof(fileList.parts));
    fileList.count = 0;
    int status;
    //int errnum = 0;
    char username[MAXBUFSIZE];
    char password[MAXBUFSIZE];
    char message[MAXBUFSIZE];
    char buffer[MAXBUFSIZE];
    char command[MAXBUFSIZE];
    char path[MAXBUFSIZE];
    char file[MAXBUFSIZE];
    int socks[4];
    int authenticated = 0;
    // int length = 0;

    char *fileName;
    // pid_t pid;
    // struct timeval timeout;

    // Make sure user passes in a .conf file
    if (argc < 2) 
    {
        perror("USAGE: dfs dfc.conf\n");
        return -1;
    }

    fileName = argv[1];
	status = readConfigFile(fileName, &confData);

    for (int i = 0; i < 4; i++) 
    {
        //Create sockets
        if((socks[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "Error creating socket\n");
            return -1;
        }

        client.sin_family = AF_INET;
        client.sin_port = htons(confData.ports[i]);
        client.sin_addr.s_addr = inet_addr(confData.serverAddr[i]);

        //Connect the socket to the server
        if (connect(socks[i], (struct sockaddr *)&client, sizeof(client)) < 0){
            close(socks[i]);
            socks[i] = -1;
        }
    }


    while(1)
    {
        memset(&message, 0, MAXBUFSIZE);
        memset(&buffer, 0, MAXBUFSIZE);
        memset(&command, 0, MAXBUFSIZE);
        memset(&path, 0, MAXBUFSIZE);
        memset(&file, 0, MAXBUFSIZE);

        if(!authenticated)
        {
            memset(&username, 0, MAXBUFSIZE);
            memset(&password, 0, MAXBUFSIZE);

            snprintf(message, MAXBUFSIZE, "%s %s", confData.username, confData.password);
            for(int i=0; i<4; i++)
            {
                send(socks[i], message, MAXBUFSIZE, 0);
                recv(socks[i], buffer, MAXBUFSIZE, 0);
                if(strcmp(buffer, "Invalid username or password.") == 0)
                {
                    printf("%s\n", buffer);
                    authenticated = 0;
                    memset(&buffer, 0, MAXBUFSIZE);
                    exit(0);
                    break;
                }
                else
                {
                    authenticated = 1;
                    memset(&buffer, 0, MAXBUFSIZE);
                }   
            }
            if(authenticated)
            {
                printf("Successfully logged in.\n");
            }
        }
        else
        {
            fflush(stdin);
            printf("Enter command (or 'exit program'): ");
            scanf("%s %s", command, file);
            fflush(stdin);
            snprintf(path, MAXBUFSIZE, "/%s/%s", confData.username, file);
            fflush(stdin);

            //Test if servers are still active
            memset(&message, 0, MAXBUFSIZE);
            memset(&buffer, 0, MAXBUFSIZE);

            snprintf(message, MAXBUFSIZE, "%s %s %s", "ping", "/", "/");
            for(int i =0; i<4; i++)
            {
                send(socks[i], message, MAXBUFSIZE, 0);
                int n = recv(socks[i], buffer, MAXBUFSIZE, 0);
                if(n <= 0)
                {
                    socks[i] = -1;
                }
            }

            if(strcmp(command, "GET") == 0 || strcmp(command, "get") == 0)
            {
                get(socks, path, file, &confData);
            }
            else if( (strcmp(command, "PUT") == 0) || (strcmp(command, "put") == 0) )
            {
                put(socks, path, file, &confData);
            }
            else if(strcmp(command, "LIST") == 0 || strcmp(command, "list") == 0)
            {
                list(socks, &fileList);
            }
            else if(strcmp(command, "exit") == 0 || strcmp(command, "EXIT") == 0)
            {
                printf("Closing sockets.\n");
                //Close and cleanup sockets
                for(int i = 0; i < 4; i++) 
                {
                    if (socks[i] != -1) 
                    {
                        close(socks[i]);
                    }   
                }
                exit(0);
                break;
            }
            else
            {
                printf("Invalid command. Use 'list', 'get', 'put', or 'exit program'\n");
            }
        }
    }

    printf("Closing socks\n");
    //Close and cleanup sockets
    for(int i = 0; i < 4; i++) {
        if (socks[i] != -1) {
            close(socks[i]);
        }   
    }
	return 0;

}