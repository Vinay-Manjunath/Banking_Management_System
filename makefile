server: server.c handler.c customer.c utils.c loan.c employee.c
	gcc server.c handler.c customer.c utils.c loan.c employee.c -o server

client: client.c
	gcc client.c -o client
