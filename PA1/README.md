# Client/Server UDP Application

## Build and make
```
$make clean ; make
$./server [PORT NUMBER]
$./client [IP ADDRESS] [PORT NUMBER]
```
If running locally, the IP Address supplied to `./client` will be `127.0.0.1`. It is recommended that you use port numbers >5000.

The client and server executables must be in different directories when you run the application, otherwise they will overwrite the files that you are trying to send/receive.

## Command list
`get [FILENAME]` - Retrieves the file from the destination location and writes it to the source location
`put [FILENAME]` - Read the file from the source caller and writes it to the destination
`ls` - Run from client side to receive list of directory files at the server
`exit` - Close both the client and the server

## Notes
The application runs on a UDP connection for message passing using recvfrom and sendto. Implemented very simplistic reliability by waiting for an ACK after each packet is sent. The sender/receiver will use select() with a timeout to wait for the ACK before sending the next packet and will try to resend if the ACK is not received.

To create a large 'dummy' file for testing transfers of >100MB, use `mkfile <size> <name>` for OS X or `xfs_mkfile <size> <name>` on Linux. For windows: ¯\_(ツ)_/¯
