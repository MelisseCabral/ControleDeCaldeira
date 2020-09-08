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

// Verifica se windows, para limpar terminal.
#ifdef _WIN32
#define CLEAR "cls"
#else  //In any unix.
#define CLEAR "clear"
#endif

#define FALHA 1
#define NSEC_PER_SEC (1000000000)

FILE * fp;

pthread_mutex_t em = PTHREAD_MUTEX_INITIALIZER;

struct timespec t;

// Comunicação socket.
int cria_socket_local(void);
void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem);
int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER);
struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino);
struct arg_struct {
    int socket_local;
    struct sockaddr_in endereco_destino;
};
char *communicate_socket(struct arg_struct *args, char *msg);

// Threads
void controle_nivel_agua();
void controle_temperatura();
void verifica_temperatura(void *arguments);
void verifica_sensores(void *arguments);
void print_sensors_status();

// Interface usuário
void show_menu(void);
void info(void);
void show_info(void);
void alarme(void);
void logErros(void);

// Controles e cálculos.
float calculo_controle_temperatura(float Kp , float Kd) ;
float controle_ni(void);
float controle_q(void);
float calculo_controle_nivel_agua(float Kp , float Kd);
float controle_na(void);
float controle_nf(void);

// Sistema.
void clear_screen(void);

// Variáveis de sistema.
int flag_alarme = 0;
int tela = 0;

// Constantes de controle.
static float S = 4184;
static float P= 1000;
static float B= 4;
static float R= 0.001;

// Valores de referência para controle.
float temp_alarme_ref = 30;
float temp_ref = 29;
static float h_ref = 2.5;
static float ni = 1;
static float na = 5;
static float nf = 50;

// Valores de sensores e atuadores.
static float ta;
static float ti;
static float temp;
static float no;
static float h;
static float q;

// Cria ponteiro e abre arquivo.
FILE *fptr;

int main(int argc, char *argv[]) {

    pthread_t t1, t2, t3, t4;

    if (argc < 3) {
        logErros();
    }
    
    int porta_destino = atoi(argv[2]);
    struct arg_struct socket_args;

    socket_args.socket_local = cria_socket_local();
    socket_args.endereco_destino = cria_endereco_destino(argv[1], porta_destino);

    pthread_create(&t1, NULL, (void *)controle_temperatura, (void *)&socket_args);
    pthread_create(&t2, NULL, (void *)controle_nivel_agua, (void *)&socket_args);
    pthread_create(&t3, NULL, (void *)verifica_temperatura, (void *)&socket_args);
    pthread_create(&t4, NULL, (void *)verifica_sensores, (void *)&socket_args);

    show_menu();
}

void show_menu(void) {
    char opcao = 0;

    //Primeira Tela.
    tela = 1;

    clear_screen();
    printf("ATENÇÃO - Alarmes só serão emitidos na tela de informação (Opção c)\n\r\n");
    printf("Valores de Referência para Controle: \n\r");
    printf("T: %.2fºC || H: %.2fm \n", temp_ref, h_ref);  //Digite a letra da opção seguida pelo valor, no caso de atuadores:
    printf("\n");
    printf("Digite a letra da opção desejada:\n");
    printf("a. Definir temperatura.\n");
    printf("b. Definir nível de água.\n");
    printf("c. Visualizar valores dos sensores.\n");
    printf("x. Terminar o programa.\n");
    printf("\n\r");
    fprintf(stderr, "Opção: \n");


    char teclado[1000];
    fgets(teclado, 1000, stdin);
    opcao = teclado[0];

    clear_screen();
    alarme();
    switch (opcao) {
        case 'a':
            // Segunda Tela.
            printf("Digite a temperatura desejada:  \n");
            scanf("%f", &temp_ref);
            break;
        case 'b':
            // Terceira Tela.
            {
                float novo_h;
                printf("Digite o valor da altura desejada (h): \n");
                scanf("%f", &novo_h);
                if(novo_h >= 0.001 && novo_h <= 3){
                    h_ref = novo_h;
                } else
                {
                    printf("\n\n\rValor para nova altura é inválido, (0.001 >= h <= 3).");
                    fflush(stdout);
                    sleep(4);
                }
                
                break;
            }
        case 'c':
            info();
            break;
        case 'x':
            exit(0);
            break;
        default:
            printf("Opção %c não existe.\n", opcao);
    }

    show_menu();
}

void info() {
    pthread_t t5;

    char parar_info = 0;
    pthread_create(&t5, NULL, (void *)print_sensors_status, NULL);
    parar_info = getchar();
    pthread_cancel(t5);
}

void show_info(void) {
    // Quarta Tela.
    tela = 4;

    clear_screen();
    alarme();
    printf("Valores de Referência para Controle: \n\r");
    printf("T: %.2fºC || H: %.2fm \n\n\r", temp_ref, h_ref);
    printf("Leitura dos Sensores: \n\r");
    printf("Ta: %.2fºC || T: %.2fºC || Ti: %.2fºC || No: %.2fkg/s || H: %.2fm\n\n\r", ta, temp, ti, no, h);
    printf("Pressione enter para voltar ao menu principal!\n");
}

char *communicate_socket(struct arg_struct *args, char *msg) {
    int nrec = 0;
    char msg_enviada[1000];
    char *msg_recebida = malloc(1000 * sizeof(char));

    strcpy(msg_enviada, msg);
    pthread_mutex_lock(&em);
    envia_mensagem(args->socket_local, args->endereco_destino, msg_enviada);
    nrec = recebe_mensagem(args->socket_local, msg_recebida, 1000);
    msg_recebida[nrec] = '\0';
    pthread_mutex_unlock(&em);

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
        // Controle de entrada de água fria.
        ni = controle_ni();
        sprintf(mensagem_enviada, "%s%.1f", "ani", ni);
        strcpy(mensagem_recebida, communicate_socket(args, mensagem_enviada));

        // Controle de entrada de calor.
        q = controle_q();
        sprintf(mensagem_enviada, "%s%.1f", "aq-", q);
        strcpy(mensagem_recebida, communicate_socket(args, mensagem_enviada));

        //this will copy the returned value of createStr() into a[]

        // Calcula início do proximo periodo.
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

float calculo_controle_temperatura(float Kp , float Kd) 
{
    // Constantes globais.
    // S, P, B, R.

    // Variáveis globais.
    // temperatura, temp, na, ni, ta ti, h.

    if(h ==0 ) h = 0.1;

    // Variáveis calculadas.
    float C = S * P * B * h;
    float qi = ni * S * (ti - temp);
    float qe = (temp - ta) / R;
    float qa = na * S * (80 - temp);
    float qt = (q + qi + qa - qe);
    float deriv_temp_sobre_tempo = (1 / C) * qt;

    // Constantes setadas experimentalmente.
    //Kp, Kd.

    //Cálculo do erro.
    float erro = (temp_ref - temp) / temp_ref;
    
    // Saída "PD".
    float output =  erro * Kp + deriv_temp_sobre_tempo * Kd; 

    return output;
}

float controle_ni(void){
    float novo_ni = calculo_controle_temperatura(-10000,1);

    if(novo_ni > 100)  novo_ni = 100;
    else if(novo_ni < 0 )  novo_ni = 0;

    return novo_ni;
}

float controle_q(void){
    float novo_q = calculo_controle_temperatura(100000000, 10000);

    if(novo_q > 1000000)  novo_q = 1000000;
    else if(novo_q < 0 )  novo_q = 0;

    return novo_q;
}


void controle_nivel_agua(void *arguments){
    struct arg_struct *args = arguments;

    long int periodo = 70000000;  // 70ms.

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

        // Controle de entrada de água aquecida a 80ºC.
        na = controle_na();
        sprintf(mensagem_enviada, "%s%.1f", "ana", na);
        strcpy(mensagem_recebida, communicate_socket(args, mensagem_enviada));

        // Controle de saída para o esgoto.
        nf = controle_nf();
        sprintf(mensagem_enviada, "%s%.1f", "anf", nf);
        strcpy(mensagem_recebida, communicate_socket(args, mensagem_enviada));

        //this will copy the returned value of createStr() into a[]

        // Calcula início do proximo periodo.
        t.tv_nsec += periodo;
        while (t.tv_nsec >= NSEC_PER_SEC) {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

float calculo_controle_nivel_agua(float Kp , float Kd){
    // Constantes globais.
    // S, P, B, R.

    // Variáveis globais.
    // temperatura, temp, na, ni, ta ti, h.

    // Variáveis calculadas.
    float deriv_vol_sobre_tempo = (1/P) * (ni + na - no - nf);
    // Dedução: V = B * h; 
    // Dedução: H = V / B 
    float deriv_altura_sobre_tempo = (1/B) * deriv_vol_sobre_tempo;

    // Constantes setadas experimentalmente.
    //Kp, Kd.

    //Cálculo do erro.
    float erro = (h_ref - h) / h_ref;
    
    // Saída "PD".
    float output =  erro * Kp + deriv_altura_sobre_tempo * Kd; 

    return output;
}

float controle_ni_nivel_agua(void){
    float novo_ni = calculo_controle_nivel_agua(10000,1);

    if(novo_ni > 100)  novo_ni = 100;
    else if(novo_ni < 0 )  novo_ni = 0;

    return novo_ni;
}

float controle_na(void){
    float novo_na = calculo_controle_nivel_agua(10000,1);

    if(novo_na > 10)  novo_na = 10;
    else if(novo_na < 0 )  novo_na = 0;

    return novo_na;
}

float controle_nf(void){
    float novo_nf = calculo_controle_nivel_agua(-10000,1);

    if(novo_nf > 100)  novo_nf = 100;
    else if(novo_nf < 0 )  novo_nf = 0;

    return novo_nf;
}

void alarme(void) {
    if (temp > temp_alarme_ref) {
        flag_alarme = 1;
        printf("%s %.2fºC\n\n\r", "Alarme! Temperatura está acima de 30ºC:", temp);
    }
}

void verifica_temperatura(void *arguments) {
    struct arg_struct *args = arguments;
    long int periodo = 10000000;  // 10ms

    // Lê a hora atual, coloca em t.
    clock_gettime(CLOCK_MONOTONIC, &t);

    // Tarefa periódica iniciará em 1 segundo.
    t.tv_sec++;

    while (1) {
        // Espera até início do próximo período

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        // Calcula início do próximo período.
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

        s = communicate_socket(args, "sno0");
        token = strtok(s, "sno");
        no = atof(token);

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