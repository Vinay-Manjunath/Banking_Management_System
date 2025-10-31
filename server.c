#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <signal.h>
#include "handler.h"

#define PORT 8080
#define MAX_SESSIONS 100

int main(){
	int serv_socket,cli_socket;
	struct sockaddr_in serv_addr,cli_addr;
	socklen_t addrlen = sizeof(cli_addr);

	//Create TCP socket
	serv_socket=socket(AF_INET,SOCK_STREAM,0);
	if(serv_socket<0){
		perror("Socket failed");
		exit(EXIT_FAILURE);
	}

	//Configure server address
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=INADDR_ANY;
	serv_addr.sin_port=htons(PORT);

	//Create shared memory for session tracking
	key_t key = ftok("sessionfile", 65);
    int shmid = shmget(key, sizeof(Session) * MAX_SESSIONS, 0666 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }

	//Attach shared memory
	Session *sessions = (Session *)shmat(shmid, NULL, 0);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].login_id[0] = '\0';
        sessions[i].role[0] = '\0';
    }
	//Detach shared memory
    shmdt(sessions);

	//Bind socket to port
	if(bind(serv_socket,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
		perror("Bind failed");
		close(serv_socket);
		exit(EXIT_FAILURE);
	}

	//Listen to incoming connections
	if(listen(serv_socket,3)<0){
		perror("Listen");
		exit(EXIT_FAILURE);
	}

	printf("Server listening on port %d\n",PORT);

	while(1){
		cli_socket=accept(serv_socket,(struct sockaddr*)&cli_addr,&addrlen);
		if(cli_socket<0){
			perror("Accept");
			exit(EXIT_FAILURE);
		}

		printf("New Connection Accepted\n");

		//Fork a new process to handle each client
		pid_t pid=fork();

		if(pid<0){
			perror("Fork failed");
			exit(EXIT_FAILURE);
		}

		if(pid==0){
			close(serv_socket);
			handle_client(cli_socket,shmid);
			close(cli_socket);
			exit(0);
		}
		else{
			close(cli_socket);
		}
	}
	close(serv_socket);
	return 0;
}