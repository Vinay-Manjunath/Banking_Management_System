#include<stdio.h>
#include<string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h> 
#include "customer.h"
#include "handler.h"
#include "utils.h"
#include <time.h>
#include <sys/socket.h> 

#define BUFFER_SIZE 1024
#define CUSTOMER_FILE "customer_credentials"
#define TRANSACTION_FILE "transactions"


// Authenticate a customer based on ID and password.
// Returns:
// 1  => success
// -1  => wrong password
// -2  => deactivated account
// 0  => customer not found or error
int authenticate_customer(char *userid, char *password, Customer *cust_buf) {
    int fd = open(CUSTOMER_FILE, O_RDONLY);
    
    if(fd == -1) {
        perror("Error opening the file");
        return 0;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock);

    Customer cust;
    int res=0;

    while(read(fd, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if(strcmp(userid, cust.cust_id) == 0) {
            if(strcmp(password, cust.password) == 0) {
                if(strcmp(cust.status,"Deactive")==0){
                    res=-2;
                    break;
                }
                memcpy(cust_buf,&cust,sizeof(Customer));
				res=1; 
            } else {
                res=-1;
            }
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    return res;
}

//Returns next customer id
int get_next_customer_id() {
    int fd = open(CUSTOMER_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        perror("Error opening customer file");
        return 1;
    }

    Customer cust;
    int last_num = 0;

    while (read(fd, &cust, sizeof(Customer)) == sizeof(Customer)) {
        int num = atoi(cust.cust_id + 1);
        if (num > last_num) last_num = num;
    }

    close(fd);
    return last_num + 1; 
}

//Create a new customer account.Create a new customer account.
int create_new_account(int sock,char *username,char *password){
    int cust_id=get_next_customer_id();

    Customer new_cust;
    memset(&new_cust,0,sizeof(Customer));

    snprintf(new_cust.cust_id,sizeof(new_cust.cust_id),"C%03d",cust_id);
    strncpy(new_cust.username,username,sizeof(new_cust.username)-1);
    strncpy(new_cust.password,password,sizeof(new_cust.password)-1);
    new_cust.amount=0.0;
    strncpy(new_cust.status,"Active",sizeof(new_cust.status)-1);

    int fd = open(CUSTOMER_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        perror("Error opening customer_credentials");
        send(sock, "Server error. Try again later.\n", strlen("Server error. Try again later.\n"), 0);
        return 0;
    }

    struct flock lock;
    off_t file_end=lseek(fd,0,SEEK_END);
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=file_end;
    lock.l_len=sizeof(Customer);
    fcntl(fd,F_SETLKW,&lock);

    if(write(fd,&new_cust,sizeof(Customer))!=sizeof(Customer)){
        perror("Error writing new customer");
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        send(sock, "Server error. Try again later.\n", 32, 0);
        return 0;
    }
    lock.l_type=F_UNLCK;
    fcntl(fd,F_SETLK,&lock);

    close(fd);

    char msg[BUFFER_SIZE];
    log_transaction(new_cust.cust_id,"Account Creation","",0);
    snprintf(msg, sizeof(msg),"Account created successfully!\nCustomer ID is %s\nInitial Balance: ₹%.2f\n\n",new_cust.cust_id, new_cust.amount);
    send(sock, msg, strlen(msg), 0);

    return 1;
}

void modify_customer_details(int sock, char *emp_id){
    int fd;
    Customer cust;
    char buffer[BUFFER_SIZE], cust_id[10],msg[BUFFER_SIZE];
    int found = 0;

    send(sock, "Enter Customer ID to modify: ", 30, 0);
    recv(sock, cust_id, sizeof(cust_id), 0);
    trim_newline(cust_id);

    fd = open(CUSTOMER_FILE, O_RDWR);
    if(fd < 0){
        perror("Error opening file");
        send(sock, "Server error.\n", 14, 0);
        return;
    }

    off_t pos = 0;
    while(read(fd, &cust, sizeof(Customer)) == sizeof(Customer)){
        if(strcmp(cust.cust_id, cust_id) == 0){
            found = 1;

            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = pos;
            lock.l_len = sizeof(Customer);
            fcntl(fd, F_SETLKW, &lock);

            char option_buf[BUFFER_SIZE];
            const char *menu = "\nSelect field to modify:\n1. Username\n2. Password\n3. Balance\nEnter choice: ";
            send(sock, menu, strlen(menu), 0);
            recv(sock, option_buf, sizeof(option_buf), 0);
            int choice = atoi(option_buf);

            switch(choice){
                case 1:
                    send(sock, "Enter new username: ", 21, 0);
                    recv(sock, cust.username, sizeof(cust.username), 0);
                    trim_newline(cust.username);
                    snprintf(msg,sizeof(msg),"Username changed by Employee %s",emp_id);
                    log_transaction(cust.cust_id, "Username Change", msg, 0);
                    break;
                case 2:
                    send(sock, "Enter new password: ", 21, 0);
                    recv(sock, cust.password, sizeof(cust.password), 0);
                    trim_newline(cust.password);
                    snprintf(msg,sizeof(msg),"Password changed by Employee %s",emp_id);
                    log_transaction(cust.cust_id, "Password Change", msg, 0);
                    break;
                case 3:
                    send(sock, "Enter new balance: ", 20, 0);
                    recv(sock, buffer, sizeof(buffer), 0);
                    cust.amount = atof(buffer);
                    snprintf(msg,sizeof(msg),"Balance Updated by Employee %s",emp_id);
                    log_transaction(cust.cust_id, "Balance Update", msg, cust.amount);
                    break;
                default:
                    send(sock, "Invalid option.\n", 16, 0);
                    break;
            }

            lseek(fd, pos, SEEK_SET);
            write(fd, &cust, sizeof(Customer));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            send(sock, "Customer details updated successfully.\n", 40, 0);
            break;
        }
        pos += sizeof(Customer);
    }

    if(!found){
        send(sock, "Customer not found.\n", 21, 0);
    }

    close(fd);
}


float check_balance(char* cust_id) {
    int fd = open(CUSTOMER_FILE, O_RDONLY);
    if (fd == -1) {
        perror("Customer file open failed");
        exit(0);
    }

    Customer cust;
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock);

    float balance = 0.0;

    while (read(fd, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (strcmp(cust.cust_id, cust_id) == 0) {
            balance = cust.amount;
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    close(fd);
    return balance;
}

//Deposit or withdraw amount from a customer's account.
int update_amount(char* cust_id,float amount){
    int fd = open(CUSTOMER_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening customer file for update");
        return 0;
    }
    Customer cust;
    off_t offset = 0;

    while (read(fd, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (strcmp(cust.cust_id, cust_id) == 0) {
            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(Customer);
            fcntl(fd, F_SETLKW, &lock);
            
            cust.amount += amount;
            lseek(fd, offset, SEEK_SET);
            write(fd, &cust, sizeof(Customer));
            fsync(fd);

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            if(amount<0){
                log_transaction(cust_id,"Withdrawal","",-amount);
            }
            else{
                log_transaction(cust_id,"Deposit","",amount);
            }

            close(fd);
            return 1;
        }
        offset += sizeof(Customer);
    }

    close(fd);
    return 0; 
}

//Transfer money between two customers with proper record locking.
int transfer_funds(int sock,char *from_user,char *to_user,float amount){
    if(amount<=0){
        char *msg="Invalid transfer amount\n";
        write(sock,msg,strlen(msg));
        return 0;
    }

    int fd=open(CUSTOMER_FILE,O_RDWR);
    if(fd<0){
        perror("Error opening customer file");
        char msg[] = "Internal error: could not open file\n";
        write(sock, msg, strlen(msg));
        return 0;
    }

    Customer cust_from,cust_to;
    off_t offset_from=-1, offset_to=-1;
    off_t offset=0;

    while(read(fd,&cust_from,sizeof(Customer))==sizeof(Customer)){
        if(strcmp(cust_from.cust_id,from_user)==0){
            offset_from=offset;
        }
        else if(strcmp(cust_from.cust_id,to_user)==0){
            cust_to=cust_from;
            offset_to=offset;
        }
        offset+=sizeof(Customer);
    }

    if(offset_from==-1){
        char msg[30];
        snprintf(msg,30,"From user not found:%s",from_user);
        write(sock,msg,strlen(msg));
        close(fd);
        return 0;
    }

    if(offset_to==-1){
        char *msg="\nTo User not found\n";
        write(sock,msg,strlen(msg));
        close(fd);
        return 0;
    }

    struct flock lock;
    off_t first_lock=offset_from<offset_to?offset_from:offset_to;
    off_t second_lock=offset_from<offset_to?offset_to:offset_from;

    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=first_lock;
    lock.l_len=sizeof(Customer);
    fcntl(fd,F_SETLKW,&lock);

    lock.l_start=second_lock;
    lock.l_len=sizeof(Customer);
    fcntl(fd,F_SETLKW,&lock);

    lseek(fd,offset_from,SEEK_SET);
    read(fd,&cust_from,sizeof(Customer));

    lseek(fd,offset_to,SEEK_SET);
    read(fd,&cust_to,sizeof(Customer));

    if(cust_from.amount < amount) {
        char *msg = "Insufficient balance\n";
        write(sock, msg, strlen(msg));

        lock.l_type = F_UNLCK;
        lock.l_start = first_lock;
        lock.l_len = sizeof(Customer);
        fcntl(fd, F_SETLK, &lock);

        lock.l_start = second_lock;
        lock.l_len = sizeof(Customer);
        fcntl(fd, F_SETLK, &lock);

        close(fd);
        return 0;
    }

    cust_from.amount-=amount;
    cust_to.amount+=amount;

    lseek(fd,offset_from,SEEK_SET);
    write(fd,&cust_from,sizeof(Customer));

    lseek(fd,offset_to,SEEK_SET);
    write(fd,&cust_to,sizeof(Customer));

    lock.l_type = F_UNLCK;
    lock.l_start = first_lock;
    lock.l_len = sizeof(Customer);
    fcntl(fd, F_SETLK, &lock);

    lock.l_start = second_lock;
    lock.l_len = sizeof(Customer);
    fcntl(fd, F_SETLK, &lock);

    close(fd);

    char desc1[100];
    snprintf(desc1,sizeof(desc1),"Transferred ₹%.2f to %s",amount,to_user);
    log_transaction(from_user, "transfer - debit", desc1, amount);

    char desc2[100];
    snprintf(desc1,sizeof(desc2),"Received ₹%.2f from %s",amount,from_user);
    log_transaction(to_user, "transfer - credit", desc2, amount);

    char msg[BUFFER_SIZE];
    snprintf(msg,sizeof(msg),"\nTransfer successful:₹%.2f sent to %s\n",amount,to_user);
    write(sock,msg,strlen(msg));

    return 1;
}

//Change customer password
int change_password(char *cust_id,char *new_pass){
    int fd=open(CUSTOMER_FILE,O_RDWR);

    if(fd<0){
        perror("Error opening customer file");
        return 0;
    }

    Customer cust;
    off_t offset=0;

    while((offset = lseek(fd, 0, SEEK_CUR)) >= 0 && read(fd,&cust,sizeof(cust))== sizeof(Customer)){
        if(strcmp(cust_id,cust.cust_id)==0){
            struct flock lock;
            lock.l_type=F_WRLCK;
            lock.l_whence=SEEK_SET;
            lock.l_len=sizeof(Customer);
            lock.l_start=offset;

            fcntl(fd,F_SETLKW,&lock);

            strcpy(cust.password,new_pass);

            lseek(fd, offset, SEEK_SET);
            write(fd,&cust,sizeof(cust));

            lock.l_type=F_UNLCK;
            fcntl(fd,F_SETLK,&lock);

            log_transaction(cust_id,"Password change","",0);

            close(fd);
            return 1;
        }
    }
    close(fd);
    return 0;
}

//Log any transaction to transaction file.
void log_transaction(char *cust_id,char *type,char *description,double amount){
    int fd=open(TRANSACTION_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) {
        perror("Error opening transaction file");
        return;
    }

    struct flock lock;
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_END;
    lock.l_start=0;
    lock.l_len=0;
    fcntl(fd,F_SETLKW,&lock);

    Transaction txn;
    off_t file_size = lseek(fd, 0, SEEK_END);
    int txn_count = file_size / sizeof(Transaction);
    snprintf(txn.trnx_id, sizeof(txn.trnx_id), "TXN%05d", txn_count + 1);
    strcpy(txn.cust_id,cust_id);
    strcpy(txn.type,type);
    strcpy(txn.description,description);
    txn.amount=amount;

    time_t now=time(NULL);
    struct tm *t=localtime(&now);
    strftime(txn.timestamp, sizeof(txn.timestamp), "%Y-%m-%d %H:%M:%S", t);

    write(fd, &txn, sizeof(Transaction));
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}

//Display all transactions of a given customer.
void display_transactions(const char *cust_id, int sock) {
    int fd = open(TRANSACTION_FILE, O_RDONLY);
    if (fd < 0) {
        char msg[] = "No transactions found.\n";
        send(sock, msg, strlen(msg), 0);
        return;
    }

    // Lock the file for reading
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    Transaction txn;
    int found = 0;

    while (read(fd, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (strcmp(txn.cust_id, cust_id) == 0) {
            char line[512];
            snprintf(line, sizeof(line),
                     "Txn ID: %s | Type: %s | Amount: %.2f | Time: %s | Description: %s\n",
                     txn.trnx_id, txn.type, txn.amount, txn.timestamp, txn.description);
            send(sock, line, strlen(line), 0); 
            found = 1;
        }
    }

    if (!found) {
        char msg[] = "No transactions available for this customer.\n";
        send(sock, msg, strlen(msg), 0);
    } else {
        char end_msg[] = "\n--- End of Transactions ---\n";
        send(sock, end_msg, strlen(end_msg), 0);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}

//Deactivate customer account
void deactivate(int sock, char *emp_id){
    int fd;
    Customer cust;
    char buffer[BUFFER_SIZE], cust_id[10],msg[BUFFER_SIZE];
    int found = 0;

    send(sock, "Enter Customer ID to deactivate: ", 35, 0);
    recv(sock, cust_id, sizeof(cust_id), 0);
    trim_newline(cust_id);

    fd = open(CUSTOMER_FILE, O_RDWR);
    if(fd < 0){
        perror("Error opening file");
        send(sock, "Server error.\n", 14, 0);
        return;
    }

    off_t pos = 0;
    while(read(fd, &cust, sizeof(Customer)) == sizeof(Customer)){
        if(strcmp(cust.cust_id, cust_id) == 0){
            found=1;
            if(strcasecmp(cust.status,"Deactive")==0){
                send(sock, "Account is already inactive.\n", 30, 0);
                break;
            }
            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = pos;
            lock.l_len = sizeof(Customer);
            fcntl(fd, F_SETLKW, &lock);

            strcpy(cust.status,"Deactive");

            snprintf(msg,sizeof(msg),"Account Deactivated by Manager %s",emp_id);
            log_transaction(cust.cust_id, "Deactivation", msg, 0);
        
            lseek(fd, pos, SEEK_SET);
            write(fd, &cust, sizeof(Customer));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            send(sock, "Account deactivated successfully.\n", 40, 0);
            break;
        }
        pos += sizeof(Customer);
    }

    if(!found){
        send(sock, "Account Deactivation Failed ", 30, 0);
    }

    close(fd);
}

//activate customer account
void activate(int sock, char *emp_id){
    int fd;
    Customer cust;
    char buffer[BUFFER_SIZE], cust_id[10],msg[BUFFER_SIZE];
    int found = 0;

    send(sock, "Enter Customer ID to activate: ", 35, 0);
    recv(sock, cust_id, sizeof(cust_id), 0);
    trim_newline(cust_id);

    fd = open(CUSTOMER_FILE, O_RDWR);
    if(fd < 0){
        perror("Error opening file");
        send(sock, "Server error.\n", 14, 0);
        return;
    }

    off_t pos = 0;
    while(read(fd, &cust, sizeof(Customer)) == sizeof(Customer)){
        if(strcmp(cust.cust_id, cust_id) == 0){
            found=1;
            if(strcasecmp(cust.status,"Active")==0){
                send(sock, "Account is already active.\n", 30, 0);
                break;
            }
            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = pos;
            lock.l_len = sizeof(Customer);
            fcntl(fd, F_SETLKW, &lock);

            strcpy(cust.status,"Active");

            snprintf(msg,sizeof(msg),"Account activated by Manager %s",emp_id);
            log_transaction(cust.cust_id, "activation", msg, 0);
        
            lseek(fd, pos, SEEK_SET);
            write(fd, &cust, sizeof(Customer));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            send(sock, "Account activated successfully.\n", 40, 0);
            break;
        }
        pos += sizeof(Customer);
    }

    if(!found){
        send(sock, "Account activation Failed ", 30, 0);
    }

    close(fd);
}
