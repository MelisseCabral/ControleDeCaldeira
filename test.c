
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t em = PTHREAD_MUTEX_INITIALIZER;

struct arg_struct { 
    int socket_local;
    int endereco_destino;
};

void verifica_temperatura(void *arguments);

void verifica_temperatura(void *arguments){
  struct arg_struct *args = arguments;
  while(1) {
		// Realiza seu trabalho.
		pthread_mutex_lock(&em);

    printf(" Id is: %d \n", args->socket_local);

		
		pthread_mutex_unlock(&em);
	}
}

int main(int argc, char *argv[]){
		struct arg_struct socket_args;
		socket_args.socket_local = 5;
		socket_args.endereco_destino = 2;

    pthread_t t1, t2, t3;
		
		pthread_create(&t3, NULL, (void *) verifica_temperatura, (void *) &socket_args);
}
