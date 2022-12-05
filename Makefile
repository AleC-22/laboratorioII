.PHONY: clean 
all: client supervisor server 
client: client.c 
	@gcc client.c -o client
	@echo "CLIENT CREATED"
server: server.c
	@gcc server.c -lrt -pthread -o server
	@echo "SERVER CREATED"	
supervisor: supervisor.c
	@gcc supervisor.c -lrt -pthread -o supervisor
	@echo "SUPERVISOR CREATED"
clean:
	@rm -f client server supervisor stdout_client.txt stdout_server.txt 
	@echo "FILES REMOVED" 
test: all
	@rm -f stdout_client.txt stdout_server.txt
	./supervisor 8 >> stdout_server.txt &
	@sleep 2
	@for number in 1 2 3 4 5 6 7 8 9 10; do \
		./client 8 5 20 >> stdout_client.txt &	\
		./client 8 5 20 >> stdout_client.txt &	\
		sleep 1 ; \
	done
	@for number in 1 2 3 4 5 6; do \
		pkill --signal SIGINT supervisor ; \
		sleep 10 ; \
	done 
	pkill --signal SIGINT supervisor
	pkill --signal SIGINT supervisor
	@sleep 2
	./misura.sh stdout_client.txt stdout_server.txt
