server: server.c
	gcc server.c -o server -pthread -g -fsanitize=address,undefined
