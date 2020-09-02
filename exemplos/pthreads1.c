/*	LIVRO FUNDAMENTOS DOS SISTEMAS DE TEMPO REAL
*
*	Ilustra a criacao de threads e uso de mutex
*	Compilar com:	gcc -lpthread -o pthreads1 pthreads1.c
*	ou
*			gcc -o pthreads1 pthreads1.c -lpthread
*
*/


#include	<pthread.h>
#include	<stdio.h>
#include	<unistd.h>
#include	"sensor.h"

/*************************************************************************
*	Monitor responsavel pelo acesso ao valor total
*
*	Uma unica funcao, usada para somar 'delta' no valor total
***/

int valor_total = 0;
pthread_mutex_t em = PTHREAD_MUTEX_INITIALIZER;

struct arg_struct { 
    int socket_local;
    int endereco_destino;
};

void verifica_temperatura(void *arguments);

int alteraValor( int delta) {
	int novo_valor;

	pthread_mutex_lock(&em);
	valor_total += delta;
	novo_valor = valor_total;
	printf("Somando %d ficará %d\n", delta, novo_valor);
	pthread_mutex_unlock(&em);
	return novo_valor;
}
/*************************************************************************/


/***
*	Codigo de thread que incrementa
***/
void thread_incrementa(void) {
	while(1){
		sleep(1);
		alteraValor(+1);
	}
}

void thread_decrementa(void) {
	while(1){
		sleep(2);
		alteraValor(-1);
	}
}

void verifica_temperatura(void *arguments){
  struct arg_struct *args = arguments;
  while(1) {
		sleep(3);
    printf(" Id is: %d \n", args->socket_local);
	}
}

int main( int argc, char *argv[]) {
		struct arg_struct socket_args;
		socket_args.socket_local = 5;
		socket_args.endereco_destino = 2;

        pthread_t t1, t2, t3, t4, t5;

        pthread_create(&t1, NULL, (void *) thread_incrementa, NULL);
        pthread_create(&t2, NULL, (void *) thread_incrementa, NULL);
        pthread_create(&t3, NULL, (void *) thread_decrementa, NULL);
        pthread_create(&t4, NULL, (void *) thread_decrementa, NULL);
        pthread_create(&t4, NULL, (void *) verifica_temperatura,  (void *) &socket_args);

	printf("Digite enter para terminar o programa:\n");
	getchar();
}








