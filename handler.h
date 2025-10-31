#ifndef HANDLER_H
#define HANDLER_H

void handle_client(int sock,int shmid);

typedef struct {
    char login_id[50];
    char role[20];
} Session;

#endif