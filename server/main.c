#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
     socket()
     bind()
     listen()
     accept()
*///

int main()
{
    char buff[1024] = "Hello, client";
    //int ret = 0;
    int netSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(7443); //Converts integer port to the appropriate data format
    serverAddress.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY = 0.0.0.0

    bind(netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    listen(netSocket, 8); //second argument is how many connections can we be waiting for

    printf("Listening...\n");
    int clientSocket = accept(netSocket, NULL, NULL); //last two params are if you want to know where the client is connecting from
    printf("Client accepted!\n");

    send(clientSocket, buff, sizeof(buff), 0); //first socket is client socket, second param is data

    close(netSocket);

    return 0;
}
