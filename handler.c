#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/shm.h>
#include "handler.h"
#include "customer.h"
#include "employee.h"
#include "utils.h"
#include "loan.h"

#define BUFFER_SIZE 1024
#define MAX_SESSIONS 100

Session *session_head = NULL; 

//Add a new session to ensure one active session per login
int add_session(int shmid, char *login_id, char *role) {
    Session *sessions = (Session *)shmat(shmid, NULL, 0);
    if (sessions == (void *)-1) return -1;

	//Check if user session already exists
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (strcmp(sessions[i].login_id, login_id) == 0 && strcmp(sessions[i].role, role) == 0) {
            shmdt(sessions);
            return 0;
        }
    }

	//Find empty slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].login_id[0] == '\0') {
            strcpy(sessions[i].login_id, login_id);
            strcpy(sessions[i].role, role);
            shmdt(sessions);
            return 1;
        }
    }

    shmdt(sessions);
    return -1; 
}

//Removes a session when user logs out
void remove_session(int shmid, char *login_id, char *role) {
    Session *sessions = (Session *)shmat(shmid, NULL, 0);
    if (sessions == (void *)-1) return;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (strcmp(sessions[i].login_id, login_id) == 0 && strcmp(sessions[i].role, role) == 0) {
            sessions[i].login_id[0] = '\0';
            sessions[i].role[0] = '\0';
            break;
        }
    }

    shmdt(sessions);
}

//handles customer login and customer operations menu
void handle_customer(int sock,int shmid){
	char* cmenu="\nEnter User ID for Login:\n";
	int temp;
	send(sock,cmenu,strlen(cmenu),0);
	char userid[BUFFER_SIZE],password[BUFFER_SIZE];
	read(sock,userid,BUFFER_SIZE);
	cmenu="\nEnter Password:\n";
	send(sock,cmenu,strlen(cmenu),0);
	read(sock,password,BUFFER_SIZE);
	trim_newline(userid);
	trim_newline(password);
	Customer cust;

	//Authenticating user for login
	int flag=authenticate_customer(userid,password,&cust);
	char* cust_id=cust.cust_id;
	
	//Username doesnot exists or wrong password
	if(flag==-1||flag==0){
		cmenu="Login Failed\n";
		send(sock,cmenu,strlen(cmenu),0);
	}
	// else if(flag==0){
	// 	char* a="\nUsername not found! Create a new account? (yes/no): ";
	// 	send(sock,a,strlen(a),0);
	// 	char buffer[BUFFER_SIZE];
	// 	read(sock,buffer,sizeof(buffer));
	// 	trim_newline(buffer);

	// 	if(strcasecmp(buffer,"yes")==0){
	// 		create_new_account(sock,userid,password);
	// 	}
	// }

	//If account is deactivated
	else if(flag==-2){
		cmenu="Account Deactivated, Please contact the manager\n";
		send(sock,cmenu,strlen(cmenu),0);
	}
	//Authentication is successful
	else{
		int add_ses=add_session(shmid,userid,"Customer");
		if(add_ses==0){
			cmenu="Please logout from previous sessions\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else if(add_ses==-1){
			cmenu="Server session limit reached\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else{
			cmenu="\nLogin Successful\n";
			send(sock,cmenu,strlen(cmenu),0);
			int option=0;
			char buffer[BUFFER_SIZE];
			int running=1;
			
			while(running){
				cmenu="\n1.View Account Balance\n2.Deposit Money\n3.Withdraw Money\n4.Transfer Funds\n5.Apply for a loan\n6.Check Loan Status\n7.Change Password\n8.Add Feedback\n9.View Transaction History\n10.Logout\n\nEnter your Choice:";
				send(sock,cmenu,strlen(cmenu),0);
				memset(buffer, 0, sizeof(buffer));
				read(sock,buffer,sizeof(option));
				option=atoi(buffer);

				int status;
				switch(option){
					//Check Balance
					case 1: float balance=check_balance(cust_id);
						char msg[128];
						snprintf(msg, sizeof(msg), "\nYour current balance is: ₹%.2f\n", balance);
						send(sock, msg, strlen(msg), 0);
						break;

					//Deposit
					case 2: char *a="Enter the amount to deposit:\n";
						send(sock,a,strlen(a),0);
						memset(buffer, 0, sizeof(buffer));
						read(sock,buffer,sizeof(buffer));
						trim_newline(buffer);
						float amount=atof(buffer);
						if(amount<0){
							a="Invalid Amount";
						}
						else{
							status=update_amount(cust_id,amount);
							if(status){
								a="Deposit Successful\n";
							}
							else{
								a="Deposit not successful\n";
							}
						}
						send(sock,a,strlen(a),0);
						break;

					//Withdrawal
					case 3: a="Enter the amount to withdraw:\n";
						send(sock,a,strlen(a),0);
						memset(buffer, 0, sizeof(buffer));
						read(sock,buffer,sizeof(buffer));
						trim_newline(buffer);
						amount=atof(buffer);
						balance=check_balance(cust_id);
						if(balance<amount){
							a="Insufficient Balance";
						}
						else{
							status=update_amount(cust_id,-amount);
							if(status){
								a="Withdrawal Successful\n";
							}
							else{
								a="Withdrawal not successful\n";
							}
						}
						send(sock,a,strlen(a),0);
						break;

					//Fund Transfer
					case 4: a="Enter the receivers customer id:\n";
						send(sock,a,strlen(a),0);
						memset(buffer,0,sizeof(buffer));
						read(sock,buffer,sizeof(buffer));
						trim_newline(buffer);
						char receiver[BUFFER_SIZE];
						strcpy(receiver,buffer);

						a="Enter the amount to transfer:\n";
						send(sock,a,strlen(a),0);
						memset(buffer,0,sizeof(buffer));
						read(sock,buffer,sizeof(buffer));
						trim_newline(buffer);
						amount=atof(buffer);

						if(amount<=0){
							a = "Invalid amount entered.\n";
							send(sock, a, strlen(a), 0);
							break;
						}
						int status=transfer_funds(sock,cust_id,receiver,amount);
						break;		

					//Apply for loan
					case 5: send(sock, "Enter loan type: ", 38, 0);
						read(sock, buffer, sizeof(buffer));
						trim_newline(buffer);
						char loan_type[20];
						strcpy(loan_type, buffer);

						send(sock, "Enter loan amount: ", 19, 0);
						read(sock, buffer, sizeof(buffer));
						amount = atof(buffer);

						if (apply_for_loan(cust_id, amount, loan_type)) {
							send(sock, "Loan request submitted successfully.\n", 37, 0);
						} else {
							send(sock, "Error submitting loan request.\n", 31, 0);
						}
						break;

					//View loan status
					case 6: view_loans(cust_id, sock);
						break;

					//Change password
					case 7: a="Enter the current password:\n";
						send(sock,a,strlen(a),0);
						char curr_pw[BUFFER_SIZE],new_pass[BUFFER_SIZE],reenter[BUFFER_SIZE];
						read(sock,curr_pw,sizeof(curr_pw));
						trim_newline(curr_pw);

						if(strcmp(curr_pw,cust.password)!=0){
							a="Wrong Password Entered\n";
							send(sock,a,strlen(a),0);
							break;
						}

						a="Enter the new password:\n";
						send(sock,a,strlen(a),0);
						read(sock,new_pass,sizeof(new_pass));
						trim_newline(new_pass);

						a="Reenter the new password:\n";
						send(sock,a,strlen(a),0);
						read(sock,reenter,sizeof(reenter));
						trim_newline(reenter);

						if(strcmp(new_pass,reenter)!=0){
							a="Both the passwords are not same\n";
							send(sock,a,strlen(a),0);
							break;
						}

						if(strcmp(new_pass,curr_pw)==0){
							a="Current and new passwords are same,please change\n";
							send(sock,a,strlen(a),0);
							break;
						}

						if(change_password(cust_id,new_pass)){
							strcpy(cust.password,new_pass);
							a="Password changed successfully\n";
							send(sock,a,strlen(a),0);
						}
						else{
							a="Password change failed\n";
							send(sock,a,strlen(a),0);
						}
						break;

					//Give feedback to employee
					case 8: give_feedback(sock,cust_id);
						break;

					//Display transactions(passbook)
					case 9: display_transactions(cust_id,sock);
						break;

					//logout
					case 10: running=0;
						remove_session(shmid,userid, "Customer");
						break;
				}
			}
		}
	}
	
}

void handle_employee(int sock,int shmid){
	char* cmenu="\nEnter Employee ID:\n";
	int temp;
	send(sock,cmenu,strlen(cmenu),0);
	char emp_id[BUFFER_SIZE],password[BUFFER_SIZE];
	memset(emp_id,0,sizeof(emp_id));
	memset(password,0,sizeof(password));
	read(sock,emp_id,BUFFER_SIZE);
	cmenu="\nEnter Password:\n";
	send(sock,cmenu,strlen(cmenu),0);
	read(sock,password,BUFFER_SIZE);
	trim_newline(emp_id);
	trim_newline(password);
	Employee emp;
	int flag=authenticate_employee(emp_id,password,&emp,0);
	
	if(flag<=0){
		cmenu="Login Failed\n";
		send(sock,cmenu,strlen(cmenu),0);
	}
	else{
		int add_ses=add_session(shmid,emp_id,"Employee");
		if(add_ses==0){
			cmenu="Please logout from previous sessions\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else if(add_ses==-1){
			cmenu="Server session limit reached\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else{
			cmenu="\nLogin Successful\n";
			send(sock,cmenu,strlen(cmenu),0);
			int option=0;
			char buffer[BUFFER_SIZE];
			int running=1;
			
			while(running){
				cmenu="\n1.Add New Customer\n2.Modify Customer Details\n3.View Assigned Loan Applications\n4.Accept or Reject Loan Application\n5.View Customer Transactions\n6.Change Password\n7.Logout\n\nEnter your Choice:";
				send(sock,cmenu,strlen(cmenu),0);
				memset(buffer, 0, sizeof(buffer));
				read(sock,buffer,sizeof(option));
				option=atoi(buffer);

				int status;

				switch(option){
					//add new customer
					case 1: char* cmenu="\nEnter Customer Name:\n";
						send(sock,cmenu,strlen(cmenu),0);
						char username[BUFFER_SIZE],cpassword[BUFFER_SIZE];
						read(sock,username,BUFFER_SIZE);
						cmenu="\nEnter Customer Password:\n";
						send(sock,cmenu,strlen(cmenu),0);
						read(sock,cpassword,BUFFER_SIZE);
						trim_newline(username);
						trim_newline(cpassword);
						create_new_account(sock,username,cpassword);
						break;

					//modify customer details
					case 2: modify_customer_details(sock,emp_id);
						break;

					//Get loans assigned to employee
					case 3: get_loans_for_employee(sock,emp_id,0);
						break;

					//Approve or Reject loan request
					case 4: process_loan_request(sock,emp_id);
						break;

					//Review Customer Transactions
					case 5: cmenu="\nEnter Customer Id:\n";
						send(sock,cmenu,strlen(cmenu),0);
						char custid[BUFFER_SIZE];
						ssize_t n = read(sock, custid, sizeof(custid)-1);
						if(n <= 0){
							send(sock, "Failed to read Customer ID.\n", 28, 0);
							break;
						}
						custid[n] = '\0';
						trim_newline(custid);
						display_transactions(custid,sock);
						break;

					//Change Password
					case 6: const char *msg = "Enter the current password:\n";
						send(sock, msg, strlen(msg), 0);

						char curr_pw[BUFFER_SIZE] = {0}, new_pass[BUFFER_SIZE] = {0}, reenter[BUFFER_SIZE] = {0};

						read(sock, curr_pw, sizeof(curr_pw));
						trim_newline(curr_pw);

						if (strcmp(curr_pw, emp.password) != 0) {
							char *a="Wrong Password Entered\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						msg = "Enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, new_pass, sizeof(new_pass));
						trim_newline(new_pass);

						msg = "Re-enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, reenter, sizeof(reenter));
						trim_newline(reenter);

						if (strcmp(new_pass, reenter) != 0) {
							char *a="Both the passwords are not same\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (strcmp(new_pass, curr_pw) == 0) {
							char *a="New password cannot be same as current password\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (change_emp_password(emp.emp_id, new_pass)) {
							strcpy(emp.password, new_pass);
							char *a="Password changed successfully\n";
							send(sock, a, strlen(a), 0);
						} else {
							char *a="Password change failed\n";
							send(sock, a, strlen(a), 0);
						}

						break;

					//Logout
					case 7: running=0;
						remove_session(shmid,emp_id, "Employee");
						break;
				}
			}
		}
	}
}
 
void handle_manager(int sock,int shmid){
	char* cmenu="\nEnter Manager ID:\n";
	int temp;
	send(sock,cmenu,strlen(cmenu),0);
	char emp_id[BUFFER_SIZE],password[BUFFER_SIZE];
	memset(emp_id,0,sizeof(emp_id));
	memset(password,0,sizeof(password));
	read(sock,emp_id,BUFFER_SIZE);
	cmenu="\nEnter Password:\n";
	send(sock,cmenu,strlen(cmenu),0);
	read(sock,password,BUFFER_SIZE);
	trim_newline(emp_id);
	trim_newline(password);
	Employee emp;
	int flag=authenticate_employee(emp_id,password,&emp,1);
	
	// if(flag==0){
	// 	char* a="\nUsername not found! Create a new account? (yes/no): ";
	// 	send(sock,a,strlen(a),0);
	// 	char buffer[BUFFER_SIZE];
	// 	read(sock,buffer,sizeof(buffer));
	// 	trim_newline(buffer);

	// 	if(strcasecmp(buffer,"yes")==0){
	// 		create_emp_new_account(sock,emp_id,password,1);
	// 	}
	// }
	if(flag<=0){
		cmenu="Login Failed\n";
		send(sock,cmenu,strlen(cmenu),0);
	}
	else{
		int add_ses=add_session(shmid,emp_id,"Manager");
		if(add_ses==0){
			cmenu="Please logout from previous sessions\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else if(add_ses==-1){
			cmenu="Server session limit reached\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else{
			cmenu="\nLogin Successful\n";
			send(sock,cmenu,strlen(cmenu),0);
			int option=0;
			char buffer[BUFFER_SIZE];
			int running=1;
			
			while(running){
				cmenu="\n1.Activate Customer Account\n2.Deactivate Cutomer Account\n3.Get All Loan Requests\n4.Assign Loan Applications to Employee\n5.Review Customer Feedback\n6.Change Password\n7.Logout\n\nEnter your Choice:";
				send(sock,cmenu,strlen(cmenu),0);
				memset(buffer, 0, sizeof(buffer));
				read(sock,buffer,sizeof(option));
				option=atoi(buffer);

				int status;

				switch(option){
					//Activate Customer account
					case 1: activate(sock,emp_id);
						break;

					//Deactivate Customer account
					case 2: deactivate(sock,emp_id);
						break;

					//View all loan requests
					case 3: get_loans_for_employee(sock,emp_id,1);
						break;

					//assign loans to employees
					case 4: assign_loan_to_employee(sock,emp_id);
						break;

					//Review Feedback
					case 5: view_employee_feedback(sock);
						break;

					//Change password
					case 6: const char *msg = "Enter the current password:\n";
						send(sock, msg, strlen(msg), 0);

						char curr_pw[BUFFER_SIZE] = {0}, new_pass[BUFFER_SIZE] = {0}, reenter[BUFFER_SIZE] = {0};

						read(sock, curr_pw, sizeof(curr_pw));
						trim_newline(curr_pw);

						if (strcmp(curr_pw, emp.password) != 0) {
							char *a="Wrong Password Entered\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						msg = "Enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, new_pass, sizeof(new_pass));
						trim_newline(new_pass);

						msg = "Re-enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, reenter, sizeof(reenter));
						trim_newline(reenter);

						if (strcmp(new_pass, reenter) != 0) {
							char *a="Both the passwords are not same\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (strcmp(new_pass, curr_pw) == 0) {
							char *a="New password cannot be same as current password\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (change_emp_password(emp.emp_id, new_pass)) {
							strcpy(emp.password, new_pass);
							char *a="Password changed successfully\n";
							send(sock, a, strlen(a), 0);
						} else {
							char *a="Password change failed\n";
							send(sock, a, strlen(a), 0);
						}

						break;

					//Logout
					case 7: running=0;
						remove_session(shmid,emp_id, "Manager");
						break;
				}
			}
		}
	}
}

void handle_admin(int sock,int shmid){
	char* cmenu="\nEnter Admin ID:\n";
	send(sock,cmenu,strlen(cmenu),0);
	char emp_id[BUFFER_SIZE],password[BUFFER_SIZE];
	memset(emp_id,0,sizeof(emp_id));
	memset(password,0,sizeof(password));
	read(sock,emp_id,BUFFER_SIZE);
	cmenu="\nEnter Password:\n";
	send(sock,cmenu,strlen(cmenu),0);
	read(sock,password,BUFFER_SIZE);
	trim_newline(emp_id);
	trim_newline(password);
	Employee emp;
	int flag=authenticate_employee(emp_id,password,&emp,2);
	
	//To create new admin
	if(flag==0){
		char* a="\nUsername not found! Create a new account? (yes/no): ";
		send(sock,a,strlen(a),0);
		char buffer[BUFFER_SIZE];
		read(sock,buffer,sizeof(buffer));
		trim_newline(buffer);
		char username[BUFFER_SIZE],pass_new[BUFFER_SIZE];
		char* cmenu="\nEnter new username:\n";
		send(sock,cmenu,strlen(cmenu),0);
		read(sock,username,BUFFER_SIZE);
	
		cmenu="\nEnter new Password:\n";
		send(sock,cmenu,strlen(cmenu),0);
		read(sock,pass_new,BUFFER_SIZE);
		trim_newline(emp_id);
		trim_newline(password);
	
		if(strcasecmp(buffer,"yes")==0){
			create_emp_new_account(sock,username,pass_new,2);
		}
	}
	else if(flag<=0){
		cmenu="Login Failed\n";
		send(sock,cmenu,strlen(cmenu),0);
	}
	else{
		int add_ses=add_session(shmid,emp_id,"Admin");
		if(add_ses==0){
			cmenu="Please logout from previous sessions\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else if(add_ses==-1){
			cmenu="Server session limit reached\n";
			send(sock,cmenu,strlen(cmenu),0);
		}
		else{
			cmenu="\nLogin Successful\n";
			send(sock,cmenu,strlen(cmenu),0);
			int option=0;
			char buffer[BUFFER_SIZE];
			int running=1;
			
			while(running){
				cmenu="\n1.Add New Employee/Manager\n2.Get All Employee Details\n3.Modify Employee Details/Roles\n4.Change Password\n5.Logout\n\nEnter your Choice:";
				send(sock,cmenu,strlen(cmenu),0);
				memset(buffer, 0, sizeof(buffer));
				read(sock,buffer,sizeof(buffer));
				trim_newline(buffer);
				option=atoi(buffer);

				int status;

				switch(option){
					//Add a new employee/manager/admin
					case 1: char* cmenu="\nEnter Employee Name:\n";
						send(sock,cmenu,strlen(cmenu),0);
						char username[BUFFER_SIZE],cpassword[BUFFER_SIZE];
						char crole[5];
						read(sock,username,BUFFER_SIZE);
						cmenu="\nEnter Employee Password:\n";
						send(sock,cmenu,strlen(cmenu),0);
						read(sock,cpassword,BUFFER_SIZE);
						cmenu="\nEnter Employee role 0 for ordinary employee 1 for manager and 2 for admin\n";
						send(sock,cmenu,strlen(cmenu),0);
						read(sock,crole,sizeof(crole));
						int role=atoi(crole);
						trim_newline(username);
						trim_newline(cpassword);
						create_emp_new_account(sock,username,cpassword,role);
						break;
					case 2: get_all_employee_details(sock);
						break;
					
					//Modify employee details or change employee roles
					case 3: modify_employee_details(sock);
						break;
					case 4: const char *msg = "Enter the current password:\n";
						send(sock, msg, strlen(msg), 0);

						char curr_pw[BUFFER_SIZE] = {0}, new_pass[BUFFER_SIZE] = {0}, reenter[BUFFER_SIZE] = {0};

						read(sock, curr_pw, sizeof(curr_pw));
						trim_newline(curr_pw);

						if (strcmp(curr_pw, emp.password) != 0) {
							char *a="Wrong Password Entered\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						msg = "Enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, new_pass, sizeof(new_pass));
						trim_newline(new_pass);

						msg = "Re-enter the new password:\n";
						send(sock, msg, strlen(msg), 0);
						read(sock, reenter, sizeof(reenter));
						trim_newline(reenter);

						if (strcmp(new_pass, reenter) != 0) {
							char *a="Both the passwords are not same\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (strcmp(new_pass, curr_pw) == 0) {
							char *a="New password cannot be same as current password\n";
							send(sock, a, strlen(a), 0);
							break;
						}

						if (change_emp_password(emp.emp_id, new_pass)) {
							strcpy(emp.password, new_pass);
							char *a="Password changed successfully\n";
							send(sock, a, strlen(a), 0);
						} else {
							char *a="Password change failed\n";
							send(sock, a, strlen(a), 0);
						}

						break;

					case 5: running=0;
						remove_session(shmid,emp_id, "Manager");
						break;
				}
			}
		}
	}	
}

//main client request handler
void handle_client(int sock, int shmid){
	char buffer[BUFFER_SIZE];

	int choice=0;
	memset(buffer,0,BUFFER_SIZE);
	
	char *menu="\n\nEnter Login Type\n\n1.Customer\n2.Employee\n3.Manager\n4.Admin\n5.Exit\n\n";
	
	int running=1;

	while(running){
		send(sock,menu,strlen(menu),0);
		read(sock, buffer, sizeof(buffer));
		trim_newline(buffer);
		int choice = atoi(buffer);

		switch(choice){
			case 1:	handle_customer(sock,shmid);
				break;
			case 2: handle_employee(sock,shmid);
				break;
			case 3: handle_manager(sock,shmid);
				break;
			case 4: handle_admin(sock,shmid);
				break;
			case 5: running=0;
				send(sock,"exit client",strlen("exit client"),0);
				break;
			default: menu="Invalid choice! Try again.\n";
				send(sock,menu,strlen(menu),0);
				break;
		}
		if(choice==5){
			break;
		}
	}
	
	close(sock);
	return;
}