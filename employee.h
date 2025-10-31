#ifndef EMPLOYEE_H

#define EMPLOYEE_H

typedef struct {
    char emp_id[10];
    char username[50];
    char password[50];
    int role;
} Employee;

typedef struct {
    char cust_id[10];     
    char emp_id[10];      
    char feedback[256];   
} Feedback;

int authenticate_employee(char *username, char *password, Employee *emp_buf,int role);
int create_emp_new_account(int sock,char *username,char *password,int role);
int change_emp_password(char *emp_id,char *new_pass);
void get_all_employee_details(int sock);
void modify_employee_details(int sock);
void view_employee_feedback(int sock);
void give_feedback(int sock, char *cust_id);

#endif