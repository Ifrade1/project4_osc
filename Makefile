all:
		gcc -o main_client main_client.c -lpthread
		gcc -o main_server main_server.c -lpthread
clean:
		rm main_client
		rm main_server

