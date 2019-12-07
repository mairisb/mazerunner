#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main()
{
    int ret = 0;
    int netSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(7443); //Converts integer port to the appropriate data format
    serverAddress.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY = 0.0.0.0

    printf("Attempting to connect...\n");
    ret = connect(netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)); //Need to cast sockaddr_int to sockaddr. Connect returns int whether it was successful or not
    if (ret < 0) {
        perror("Error connecting to server: ");
        exit(1);
    }
    printf("Connection established!\n");

    char buff[1024] = "";
    recv(netSocket, &buff, sizeof(buff), 0); //There is an optional flags param as well
    printf("Message from server: %s\n", buff);

    close(netSocket); //Remember to close

    return 0;
}
