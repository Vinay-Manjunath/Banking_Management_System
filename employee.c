#include<stdio.h>
#include<string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h> 
#include "Employee.h"
#include "handler.h"
#include "utils.h"
#include <time.h>
#include <sys/socket.h> 

#define BUFFER_SIZE 1024
#define EMPLOYEE_FILE "employee_details"
#define FEEDBACK_FILE "feedback"

// Verify login credentials for an employee based on role and password.
// 1  -> successful authentication
// -1  -> lower privilege role trying to access higher role
// -2  -> incorrect password
// 0  -> employee not found / file error
int authenticate_employee(char *emp_id, char *password, Employee *emp_buf,int role) {
    int fd = open(EMPLOYEE_FILE, O_RDONLY);
    
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

    Employee emp;
    int res=0;

    while(read(fd, &emp, sizeof(Employee))!=0) {
        if(strcmp(emp_id, emp.emp_id) == 0) {
            if((strcmp(password, emp.password) == 0) && (emp.role == role || (role==0 && emp.role >= role))) {
                memcpy(emp_buf,&emp,sizeof(Employee));
				res=1; 
            }
            else if(role>emp.role){
                res=-1;
            } 
            else {
                res=-2;
            }
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    return res;
}

//Generates next employee ID by reading existing records.
int get_next_employee_id() {
    int fd = open(EMPLOYEE_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        perror("Error opening Employee file");
        return 1;
    }

    Employee emp;
    int last_num = 0;

    while (read(fd, &emp, sizeof(Employee)) == sizeof(Employee)) {
        int num = atoi(emp.emp_id + 1);
        if (num > last_num) last_num = num;
    }

    close(fd);
    return last_num + 1; 
}

int create_emp_new_account(int sock,char *username,char *password,int role){
    int emp_id=get_next_employee_id();

    Employee new_emp;
    memset(&new_emp,0,sizeof(Employee));

    snprintf(new_emp.emp_id,sizeof(new_emp.emp_id),"E%03d",emp_id);
    strncpy(new_emp.username,username,sizeof(new_emp.username)-1);
    strncpy(new_emp.password,password,sizeof(new_emp.password)-1);
    new_emp.role=role;

    int fd = open(EMPLOYEE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        perror("Error opening Employee_credentials");
        send(sock, "Server error. Try again later.\n", strlen("Server error. Try again later.\n"), 0);
        return 0;
    }

    struct flock lock;
    off_t file_end=lseek(fd,0,SEEK_END);
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=file_end;
    lock.l_len=sizeof(Employee);
    fcntl(fd,F_SETLKW,&lock);

    if(write(fd,&new_emp,sizeof(Employee))!=sizeof(Employee)){
        perror("Error writing new Employee");
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
    snprintf(msg, sizeof(msg),"Employee Account created successfully!\nNew Employee ID %s\n",new_emp.emp_id);
    send(sock, msg, strlen(msg), 0);

    return 1;
}

int change_emp_password(char *emp_id, char *new_pass) {
    int fd = open(EMPLOYEE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening employee file");
        return 0;
    }

    Employee emp;
    off_t offset = 0;

    while (read(fd, &emp, sizeof(Employee)) == sizeof(Employee)) {
        if (strcmp(emp.emp_id, emp_id) == 0) {
            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(Employee);
            fcntl(fd, F_SETLKW, &lock);

            strcpy(emp.password, new_pass);

            lseek(fd, offset, SEEK_SET);
            write(fd, &emp, sizeof(Employee));

            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);
            close(fd);
            return 1;
        }
        offset += sizeof(Employee);
    }

    close(fd);
    return 0;
}

void get_all_employee_details(int sock){
    int fd = open(EMPLOYEE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening employee file");
        return;
    }

    Employee emp;
    off_t offset = 0;
    while(read(fd,&emp,sizeof(Employee))==sizeof(Employee)){
        char msg[BUFFER_SIZE];
        char emprole[20];
        if(emp.role==0){
            strcpy(emprole,"Employee");
        }
        else if(emp.role==1){
            strcpy(emprole,"Manager");
        }
        else if(emp.role==2){
            strcpy(emprole,"Admin");
        }
        else{
            strcpy(emprole,"Unknown");
        }
        snprintf(msg,sizeof(msg),"Employee Id:%s| Username:%s | Role:%s\n",emp.emp_id,emp.username,emprole);
        send(sock,msg,strlen(msg),0);
    }
    close(fd);

}

void modify_employee_details(int sock) {
    char emp_id[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    const char *msg = "Enter Employee ID to modify:\n";
    send(sock, msg, strlen(msg), 0);
    memset(emp_id, 0, sizeof(emp_id));
    read(sock, emp_id, sizeof(emp_id));
    trim_newline(emp_id);

    int fd = open(EMPLOYEE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening employee file");
        return;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    Employee emp;
    off_t pos = 0;
    int found = 0;

    while (read(fd, &emp, sizeof(Employee)) == sizeof(Employee)) {
        if (strcmp(emp.emp_id, emp_id) == 0) {
            found = 1;
            break;
        }
        pos += sizeof(Employee);
    }

    if (!found) {
        const char *notfound = "Employee not found.\n";
        send(sock, notfound, strlen(notfound), 0);
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        return;
    }

    snprintf(buffer, sizeof(buffer),
             "\nCurrent Details:\nUsername: %s\nPassword: %s\nRole: %d\n",
             emp.username, emp.password, emp.role);
    send(sock, buffer, strlen(buffer), 0);

    msg = "\nWhat do you want to modify?\n1. Username\n2. Password\n3. Role\nEnter choice: ";
    send(sock, msg, strlen(msg), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    int choice = atoi(buffer);

    switch (choice) {
        case 1:
            msg = "Enter new username:\n";
            send(sock, msg, strlen(msg), 0);
            memset(emp.username, 0, sizeof(emp.username));
            read(sock, emp.username, sizeof(emp.username));
            trim_newline(emp.username);
            break;
        case 2:
            msg = "Enter new password:\n";
            send(sock, msg, strlen(msg), 0);
            memset(emp.password, 0, sizeof(emp.password));
            read(sock, emp.password, sizeof(emp.password));
            trim_newline(emp.password);
            break;
        case 3:
            msg = "Enter new role (0=Employee, 1=Manager, 2=Admin):\n";
            send(sock, msg, strlen(msg), 0);
            memset(buffer, 0, sizeof(buffer));
            read(sock, buffer, sizeof(buffer));
            emp.role = atoi(buffer);
            break;
        default:
            msg = "Invalid choice.\n";
            send(sock, msg, strlen(msg), 0);
            break;
    }

    lseek(fd, pos, SEEK_SET);
    write(fd, &emp, sizeof(Employee));

    const char *success = "Employee details updated successfully!\n";
    send(sock, success, strlen(success), 0);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}

void give_feedback(int sock, char *cust_id) {
    int fd = open(EMPLOYEE_FILE, O_RDONLY);
    if(fd < 0){
        perror("Error opening employee file");
        send(sock, "Unable to fetch employee list.\n", 32, 0);
        return;
    }

    Employee emp;
    char msg[512];

    send(sock, "\nAvailable Employees/Managers:\n", 32, 0);
    while(read(fd, &emp, sizeof(emp)) == sizeof(emp)) {
        char *role_str;
        if(emp.role == 0) role_str = "Employee";
        else if(emp.role == 1) role_str = "Manager";
        else role_str = "Admin";

        snprintf(msg, sizeof(msg), "ID: %s | Username: %s | Role: %s\n", emp.emp_id, emp.username, role_str);
        send(sock, msg, strlen(msg), 0);
    }
    close(fd);

    char emp_id[10], feedback_text[256];
    send(sock, "\nEnter Employee/Manager ID to give feedback: ", 45, 0);
    memset(emp_id, 0, sizeof(emp_id));
    recv(sock, emp_id, sizeof(emp_id), 0);
    trim_newline(emp_id);

    send(sock, "Enter your feedback: ", 21, 0);
    memset(feedback_text, 0, sizeof(feedback_text));
    recv(sock, feedback_text, sizeof(feedback_text), 0);
    trim_newline(feedback_text);

    fd = open(FEEDBACK_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        perror("Error opening feedback file");
        send(sock, "Failed to save feedback.\n", 25, 0);
        return;
    }

    Feedback fb;
    strcpy(fb.cust_id, cust_id);
    strcpy(fb.emp_id, emp_id);
    strncpy(fb.feedback, feedback_text, sizeof(fb.feedback)-1);
    fb.feedback[sizeof(fb.feedback)-1] = '\0';

    write(fd, &fb, sizeof(fb));
    close(fd);

    send(sock, "Feedback submitted successfully.\n", 32, 0);
}


void view_employee_feedback(int sock){
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if(fd < 0){
        perror("Error opening feedback file");
        send(sock, "No feedback available.\n", 23, 0);
        return;
    }

    Feedback fb;
    int found=0;
    char msg[BUFFER_SIZE];
    char emp_id[BUFFER_SIZE];
    
    send(sock,"Enter employee id:",18,0);
    recv(sock,emp_id,sizeof(emp_id),0);

    while(read(fd, &fb, sizeof(fb)) == sizeof(fb)){
        if(strcmp(fb.emp_id,emp_id)==0){
            found=1;
            snprintf(msg, sizeof(msg), "\nCustomer ID: %s | Feedback: %s\n",fb.cust_id, fb.feedback);
            send(sock, msg, strlen(msg), 0);
        }
    }
    if(!found){
        send(sock, "No feedback available.\n", 23, 0);
    }

    close(fd);
}
