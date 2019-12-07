#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

int main(int argc, char** argv) {
    int ret = 0;
    int netSock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(7443); //Converts integer port to the appropriate data format
    servAddr.sin_addr.s_addr = inet_addr(argv[1]);

    printf("Attempting to connect...\n");
    //Need to cast sockaddr_int to sockaddr. Connect returns int whether it was successful or not
    ret = connect(netSock, (struct sockaddr *) &servAddr, sizeof(servAddr));
    if (ret < 0) {
        perror("Error connecting to server: ");
        exit(1);
    }
    printf("Connection established!\n");

    char buff[1024] = "";
    recv(netSock, &buff, sizeof(buff), 0); //There is an optional flags param as well
    printf("Message from server: %s\n", buff);

    close(netSock); //Remember to close

    return 0;
}
