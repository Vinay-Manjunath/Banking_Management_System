#ifndef CUSTOMER_H
#define CUSTOMER_H

typedef struct {
    char cust_id[10];   
    char username[50];
    char password[50];
    double amount; 
    char status[10];
} Customer;

typedef struct{
    char trnx_id[10];
    char cust_id[10];
    char type[20];
    char description[100];
    double amount;
    char timestamp[30];
}Transaction;

int authenticate_customer(char *username, char *password, Customer *cust_buf);
int create_new_account(int sock,char *username,char *password);
float check_balance(char* username);
void log_transaction(char *cust_id,char *type,char *description,double amount);
int update_amount(char* username,float amount);
int transfer_funds(int sock,char *from_user,char *to_user,float amount);
int change_password(char *cust_id,char *new_pass);
void display_transactions(const char *cust_id, int sock);
void modify_customer_details(int sock, char *emp_id);
void deactivate(int sock, char *emp_id);
void activate(int sock, char *emp_id);

#endif