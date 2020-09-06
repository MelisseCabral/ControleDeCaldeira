/*  Programa de demonstração de uso de sockets UDP em C no Linux.
 *  Funcionamento:
 *  Usuário escolhe opção no menu e então envia uma msg para a caldeira.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#define CLEAR "cls"
#else  //In any unix.
#define CLEAR "clear"
#endif

#define FALHA 1
#define NSEC_PER_SEC (1000000000)
#define MSG_ALARME "Alarme! Temperatura está acima de 30ºC:"

// Constantes do Sistema.
#define S 4184
#define P 1000
#define B 4
#define R 0.001

pthread_mutex_t em = PTHREAD_MUTEX_INITIALIZER;
static double limite_atual = 30;

struct timespec t;
struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino);

struct arg_struct {
    int socket_local;
    struct sockaddr_in endereco_destino;
};

void controle_nivel_agua();
void controle_temperatura();
void verifica_temperatura(void *arguments);
void verifica_sensores(void *arguments);
char *communicate_socket(struct arg_struct *args, char *msg);

void show_menu(void *socket_args);
void info(void *socket_args);
void show_info(void);
void print_sensors_status();
void alarme(void);
int cria_socket_local(void);
void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem);
int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER);
void logErros(void);
void clear_screen(void);
float calculo_controle_temperatura();

//Valores de Referência para controle
static float temperatura = 20;
static float ni = 1;
static float na = 5;
static float nf = 50;

// Valores Sensores e atuadores
static float ta;
static float ti;
static float temp;
static float no;
static float h;
static float q;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        logErros();
    }
    int porta_destino = atoi(argv[2]);
    struct arg_struct socket_args;
    pthread_t t1, t2, t3, t4;

    socket_args.socket_local = cria_socket_local();
    socket_args.endereco_destino = cria_endereco_destino(argv[1], porta_destino);

    pthread_create(&t1, NULL, (void *)controle_temperatura, (void *)&socket_args);
    pthread_create(&t2, NULL, (void *)controle_nivel_agua, (void *)&socket_args);
    pthread_create(&t3, NULL, (void *)verifica_temperatura, (void *)&socket_args);
    pthread_create(&t4, NULL, (void *)verifica_sensores, (void *)&socket_args);

    show_menu((void *)&socket_args);
}
void show_menu(void *socket_args) {
    char opcao = 0;
    while (opcao == 0) {
        //Primeira Tela.
        clear_screen();
        printf("Valores de Referência para Controle: \n\r");
        printf("T: %.2fºC || Ni: %.2fkg/s || Na: %.2fkg/s || Nf: %.2fkg/s \n", temperatura, ni, na, nf);  //Digite a letra da opção seguida pelo valor, no caso de atuadores:
        printf("\n");
        printf("Digite a letra da opção desejada:\n");
        printf("a. Definir temperatura.\n");
        printf("b. Definir nível de água.\n");
        printf("c. Visualizar valores dos sensores.\n");
        printf("x. Terminar o programa.\n");
        printf("\n\r");
        printf("Opção: ");

        char teclado[1000];
        fgets(teclado, 1000, stdin);
        opcao = teclado[0];
    }

    //Segunda Tela.
    clear_screen();
    switch (opcao) {
        case 'a':
            printf("Digite a temperatura desejada: ");
            scanf("%f", &temperatura);
            break;
        case 'b':
            printf("Digite o valor de Ni: ");
            scanf("%f", &ni);
            printf("Digite o valor de Na: ");
            scanf("%f", &na);
            printf("Digite o valor de Nf: ");

            scanf("%f", &nf);
            break;
        case 'c':
            info((void *)&socket_args);
            break;
        case 'x':
            exit(0);
            break;
        default:
            printf("Opção %c não existe.\n", opcao);
    }

    show_menu((void *)&socket_args);
}

void info(void *socket_args) {
    pthread_t t5;

    char parar_info = 0;
    while (parar_info == 0) {
        pthread_create(&t5, NULL, (void *)print_sensors_status, NULL);
        parar_info = getchar();
    }
    pthread_cancel(t5);
}

void show_info(void) {
    //Terceira Tela.
    clear_screen();
    printf("Valores de Referência para Controle: \n\r");
    printf("T: %.2fºC || Na: %.2fºC || Ni: %.2fºC || Nf: %.2fkg/s\n\n\r", temperatura, na, ni, nf);
    printf("Leitura dos Sensores: \n\r");
    printf("Ta: %.2fºC || T: %.2fºC || Ti: %.2fºC || No: %.2fkg/s || H: %.2fm\n\n\r", ta, temp, ti, no, h);
    printf("Pressione enter para voltar ao menu principal!\n");
}

char *communicate_socket(struct arg_struct *args, char *msg) {
    int nrec = 0;
    char msg_enviada[1000];
    char *msg_recebida = malloc(1000 * sizeof(char));

    strcpy(msg_enviada, msg);
    envia_mensagem(args->socket_local, args->endereco_destino, msg_enviada);
    nrec = recebe_mensagem(args->socket_local, msg_recebida, 1000);
    msg_recebida[nrec] = '\0';

    return msg_recebida;
}

void controle_temperatura(void *arguments) {
    struct arg_struct *args = arguments;

    long int periodo = 50000000;  // 50ms.

    // Le a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);

    // Tarefa periódica iniciará em 1 segundo.
    t.tv_sec++;

    while (1) {
        // Espera até início do próximo período.
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        char mensagem_recebida[1000];
        char mensagem_enviada[1000];

        // Realiza seu trabalho.
        pthread_mutex_lock(&em);

        na += calculo_controle_temperatura();
        sprintf(mensagem_enviada, "%s%.2f", "ana", na);
        strcpy(mensagem_recebida, communicate_socket(args, mensagem_enviada));

        //this will copy the returned value of createStr() into a[]
        pthread_mutex_unlock(&em);

        // Calcula início do proximo periodo.
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

float calculo_controle_temperatura() {
    // Variáveis globais.
    // Na,

    // Variáveis calculadas.
    float C = S * P * B * h;
    float qi = ni * S * (ti - temp);
    float qe = (temp - ta) / R;
    float qa = na * S * (80 - temp);
    float qt = (q + qi + qa - qe);
    float derivada_temperatura_sobre_tempo = (1 / C) * qt;

    // Constantes setadas experimentalmente.
    float Kp = 1;
    float Kd = .1;

    float erro = (temp - temperatura) / temperatura;

    //Saída;
    float output = na * (derivada_temperatura_sobre_tempo * Kd + Kp) * erro;
    printf("%.2f \n ", derivada_temperatura_sobre_tempo);
    return output;
}

void controle_nivel_agua(void *arguments) {
    struct arg_struct *args = arguments;
    long int periodo = 70000000;  // 70ms

    // Lê a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);

    // Tarefa periodica iniciará em 1 segundo.
    t.tv_sec++;

    while (1) {
        // Espera ateh início do proximo período.
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        pthread_mutex_lock(&em);
        // Realiza seu trabalho.
        /*
        char s[1000];
        sprintf(s, "%s%.2f", "ani", ni);
        communicate_socket(args, s);

        sprintf(s, "%s%.2f", "ana", na);
        communicate_socket(args, s);

        sprintf(s, "%s%.2f", "anf", nf);
        communicate_socket(args, s);
        */

        pthread_mutex_unlock(&em);

        // Calcula início do próximo período.
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

void alarme(void) {
    if (temp > limite_atual) {
        printf("%s %.2fºC\n\n\r", MSG_ALARME, temp);
    }
}

void verifica_temperatura(void *arguments) {
    struct arg_struct *args = arguments;
    long int periodo = 10000000;  // 10ms

    // Lê a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);

    // Tarefa periodica iniciará em 1 segundo.
    t.tv_sec++;

    while (1) {
        // Espera ateh inicio do proximo periodo

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        pthread_mutex_lock(&em);
        alarme();
        pthread_mutex_unlock(&em);

        // Calcula inicio do proximo periodo
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

void print_sensors_status() {
    long int periodo = 10000000;  // 10ms
    // Lê a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);
    t.tv_sec++;

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        show_info();

        // Calcula inicio do proximo periodo
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

void verifica_sensores(void *arguments) {
    struct arg_struct *args = arguments;
    long int periodo = 10000000;  // 10ms

    // Lê a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);

    // Tarefa periodica iniciará em 1 segundo.
    t.tv_sec++;

    while (1) {
        // Espera ateh inicio do proximo periodo
        char *s;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        pthread_mutex_lock(&em);

        s = communicate_socket(args, "sta0");
        char *token = strtok(s, "sta");
        ta = atof(token);

        s = communicate_socket(args, "sti0");
        token = strtok(s, "sti");
        ti = atof(token);

        s = communicate_socket(args, "st-0");
        char *b = s + 3;
        temp = atof(b);

        s = communicate_socket(args, "sh-0");
        token = strtok(s, "sh-");
        h = atof(token);

        pthread_mutex_unlock(&em);

        // Calcula inicio do proximo periodo
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

void clear_screen(void) {
    system(CLEAR);
}

int cria_socket_local(void) {
    int socket_local;  // Socket usado na comunicacao.

    socket_local = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_local < 0) {
        perror("socket");
        return -1;
    }
    return socket_local;
}

void logErros(void) {
    fprintf(stderr, "Uso: controlemanual <endereco> <porta>.\n");
    fprintf(stderr, "<endereco> é o endereco IP da caldeira.\n");
    fprintf(stderr, "<porta> é o número da porta UDP da caldeira.\n");
    fprintf(stderr, "Exemplo de uso:\n");
    fprintf(stderr, "   controlemanual localhost 12345\n");
    exit(FALHA);
}

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino) {
    struct sockaddr_in servidor;    // Endereco do servidor incluindo ip e porta.
    struct hostent *dest_internet;  // Endereco destino em formato proprio.
    struct in_addr dest_ip;         // Endereco destino em formato ip numerico.

    if (inet_aton(destino, &dest_ip))
        dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
    else
        dest_internet = gethostbyname(destino);

    if (dest_internet == NULL) {
        fprintf(stderr, "Endereço de rede inválido.\n");
        exit(FALHA);
    }

    memset((char *)&servidor, 0, sizeof(servidor));
    memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(porta_destino);

    return servidor;
}

void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem) {
    // Envia msg ao servidor.

    if (sendto(socket_local, mensagem, strlen(mensagem) + 1, 0, (struct sockaddr *)&endereco_destino, sizeof(endereco_destino)) < 0) {
        perror("sendto");
        return;
    }
}

int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER) {
    int bytes_recebidos;  // Número de bytes recebidos.

    // Espera pela mensagem de resposta do servidor.
    bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
    if (bytes_recebidos < 0) {
        perror("recvfrom");
    }

    return bytes_recebidos;
}