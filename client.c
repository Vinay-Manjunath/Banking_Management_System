#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int main(){
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    int choice;

    // Create TCP socket
    sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(PORT);
    serv_addr.sin_addr.s_addr=INADDR_ANY; //localhost

    if(connect(sock,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    //Server-client communication loop
    while(1){
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n;

        //Data from server
        while ((n = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
            fflush(stdout);

            if (n < BUFFER_SIZE - 1)
                break;
        }
    
        //Getting User input
        fflush(stdout);
        if (!fgets(buffer, BUFFER_SIZE, stdin))
            break;
        buffer[strcspn(buffer, "\n")] = 0;
        
        //Exit client if server sends "exit client"
        if(strcmp(buffer,"exit client")==0){
            printf("Exiting client...\n");
            break;
        }
        send(sock, buffer, strlen(buffer), 0);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}