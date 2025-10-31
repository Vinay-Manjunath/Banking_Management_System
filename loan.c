#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h> 
#include "loan.h"
#include "utils.h"
#include "customer.h"

#define LOAN_FILE "loan_records"
#define BUFFER_SIZE 1024

typedef struct{
    char loan_id[10];
    char cust_id[10];
    char emp_id[10];
    double amount;
    char type[20];
    char status[10];
} Loan;

//Generate the next available loan ID
int get_next_loan_id(){
    int fd=open(LOAN_FILE,O_RDONLY|O_CREAT,0644);
    if(fd<0){
        perror("Error opening loan file");
        return 1;
    }

    Loan loan;
    int last_id=0;

    while(read(fd,&loan,sizeof(loan))==sizeof(loan)){
        int id_num=atoi(loan.loan_id+1);
        if(id_num>last_id) last_id=id_num;
    }
    close(fd);
    return last_id+1;
}

//Allow a customer to apply for a new loan
int apply_for_loan(char *cust_id,double amount,char *type){
    int fd=open(LOAN_FILE,O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd<0){
        perror("Error opening loan file");
        return 0;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_END;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    Loan loan;
    memset(&loan,0,sizeof(loan));
    int next_id=get_next_loan_id();
    snprintf(loan.loan_id,sizeof(loan.loan_id),"L%03d",next_id);
    strncpy(loan.cust_id,cust_id,sizeof(loan.cust_id)-1);
     strncpy(loan.emp_id,"",sizeof(loan.cust_id)-1);
    loan.amount = amount;
    strncpy(loan.type, type, sizeof(loan.type) - 1);
    strncpy(loan.status, "pending", sizeof(loan.status) - 1);

    write(fd,&loan,sizeof(Loan));
    fsync(fd);

    lock.l_type=F_UNLCK;
    fcntl(fd,F_SETLK,&lock);

    log_transaction(cust_id,"Loan Request",type,amount);

    close(fd);
    return 1;
}

//Display all loans belonging to a specific customer.
void view_loans(const char *cust_id, int sock) {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Error opening loan file");
        return;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    Loan loan;
    char msg[256];
    int found = 0;

    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (strcmp(loan.cust_id, cust_id) == 0) {
            snprintf(msg, sizeof(msg),
                     "Loan ID: %s | Assigned Employee: %s | Type: %s | Amount: ₹%.2f | Status: %s\n",
                     loan.loan_id, loan.emp_id, loan.type, loan.amount, loan.status);
            send(sock, msg, strlen(msg), 0);
            found = 1;
        }
    }

    if (!found) {
        send(sock, "No loans found.\n", strlen("No loans found.\n"), 0);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    close(fd);
}

// Retrieve all loans assigned to an employee or all loans if 'flag' is set (used by managers).
void get_loans_for_employee(int sock,char *emp_id,int flag){
    int fd = open(LOAN_FILE, O_RDONLY);
    if(fd < 0){
        perror("Error opening loan file");
        send(sock, "Server error.\n", 14, 0);
        return;
    }

    Loan loan;
    char msg[BUFFER_SIZE];
    int found = 0;

    while(read(fd,&loan,sizeof(loan))==sizeof(Loan)){
        if(flag||(strcmp(loan.emp_id, emp_id) == 0)){
            found = 1;
            if(flag){
                snprintf(msg, sizeof(msg),"Loan ID: %s | Employee ID: %s | Customer ID: %s | Amount: %.2f | Type: %s | Status: %s\n",loan.loan_id, loan.emp_id,loan.cust_id, loan.amount, loan.type, loan.status);
            }
            else{
                snprintf(msg, sizeof(msg),"Loan ID: %s | Customer ID: %s | Amount: %.2f | Type: %s | Status: %s\n",loan.loan_id,loan.cust_id, loan.amount, loan.type, loan.status);    
            }
            send(sock, msg, strlen(msg), 0);
        }
    }
    if(!found){
        send(sock, "No loans assigned.\n", 19, 0);
    }

    close(fd);
}

// Allows employee to accept or reject loans
void process_loan_request(int sock, const char *emp_id){
    int fd = open(LOAN_FILE, O_RDWR);
    if(fd < 0){
        perror("Error opening loan file");
        send(sock, "Server error.\n", 14, 0);
        return;
    }

    char loan_id[10];
    char choice_buf[BUFFER_SIZE];
    send(sock, "Enter Loan ID to process: ", 27, 0);
    recv(sock, loan_id, sizeof(loan_id), 0);
    loan_id[strcspn(loan_id, "\n")] = 0;

    Loan loan;
    off_t pos = 0;
    int found = 0;

    while(read(fd, &loan, sizeof(Loan)) == sizeof(Loan)){
        if(strcmp(loan.loan_id, loan_id) == 0){
            found = 1;
            char cust_id[BUFFER_SIZE];
            strcpy(cust_id,loan.cust_id);

            if(strcmp(loan.emp_id, emp_id) != 0){
                send(sock, "You are not assigned to this loan.\n", 36, 0);
                break;
            }

            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = pos;
            lock.l_len = sizeof(Loan);
            fcntl(fd, F_SETLKW, &lock);
            char msg[BUFFER_SIZE];

            send(sock, "Enter 1 to Approve, 2 to Reject: ", 35, 0);
            recv(sock, choice_buf, sizeof(choice_buf), 0);
            int choice = atoi(choice_buf);

            if(choice == 1){
                strcpy(loan.status, "Approved");
                snprintf(msg,sizeof(msg),"Loan %s approved by Employee %s",loan_id,emp_id);
                log_transaction(cust_id,"Loan Approval",msg,0);
            } else if(choice == 2){
                strcpy(loan.status, "Rejected");
                snprintf(msg,sizeof(msg),"Loan %s rejected by Employee %s",loan_id,emp_id);
                log_transaction(cust_id,"Loan Approval",msg,0);
            } else{
                send(sock, "Invalid choice.\n", 16, 0);
                lock.l_type = F_UNLCK;
                fcntl(fd, F_SETLK, &lock);
                break;
            }

            lseek(fd, pos, SEEK_SET);
            write(fd, &loan, sizeof(Loan));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            send(sock, "Loan request updated successfully.\n", 36, 0);
            break;
        }
        pos += sizeof(Loan);
    }

    if(!found){
        send(sock, "Loan not found.\n", 16, 0);
    }

    close(fd);
}

//Manager assigns a pending loan to an employee.
void assign_loan_to_employee(int sock,char *manager_id){
    int fd;
    Loan loan;
    char loan_id[10],emp_id[10];
    int found=0;
    char msg[BUFFER_SIZE];

    memset(loan_id,0,sizeof(loan_id));
    memset(emp_id,0,sizeof(emp_id));
    send(sock,"Enter Loan ID to assign:",40,0);
    recv(sock,loan_id,sizeof(loan_id),0);
    trim_newline(loan_id);

    send(sock,"Enter Employee ID to assign this Loan to:",50,0);
    recv(sock,emp_id,sizeof(emp_id),0);
    trim_newline(emp_id);

    fd = open(LOAN_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening loan file");
        send(sock, "Server error.\n", strlen("Server error.\n"), 0);
        return;
    }

    off_t pos=0;
    while(read(fd,&loan,sizeof(loan))==sizeof(Loan)){
        if(strcmp(loan.loan_id,loan_id)==0){
            found=1;

            struct flock lock = {F_WRLCK, SEEK_SET, pos, sizeof(Loan), 0};
            fcntl(fd, F_SETLKW, &lock);

            strcpy(loan.emp_id, emp_id);
            strcpy(loan.status, "Assigned");

            lseek(fd, pos, SEEK_SET);
            write(fd, &loan, sizeof(Loan));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            snprintf(msg, sizeof(msg), "Loan %s assigned to Employee %s by Manager %s", loan.loan_id, emp_id, manager_id);
            log_transaction(loan.cust_id, "LoanAssignment", msg, loan.amount);

            send(sock, "Loan assigned successfully.\n", strlen("Loan assigned successfully.\n"), 0);
            break;
        }
        pos += sizeof(Loan);
    }

    if (!found) {
        snprintf(msg,sizeof(msg),"Loan ID not found.\n");
        send(sock,msg,sizeof(msg), 0);
    }

    close(fd);
}