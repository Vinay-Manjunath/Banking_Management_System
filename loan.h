#ifndef LOAN_H

#define LOAN_H

int apply_for_loan(char *cust_id,double amount,char *type);
void view_loans(const char *cust_id, int sock);
void get_loans_for_employee(int sock,char *emp_id,int flag);
void process_loan_request(int sock, const char *emp_id);
void assign_loan_to_employee(int sock,char *manager_id);

#endif