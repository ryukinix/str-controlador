/*  Programa de demonstracao de uso de sockets UDP em C no Linux
 *  Funcionamento:
 *  Usuario escolhe opcao no menu e entao envia uma msg para a caldeira.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <ncurses.h>
#include <string.h>

#define FALHA 1
#define NUM_THREADS 6
#define NSEC_PER_SEC 1000000000
#define HISTORICO_ARQUIVO "historico.txt"

pthread_mutex_t lock;

// ------------------------------------------

// Vetor de sensores
float VS[5];
float VA[4];

// Estrutura de argumentos para passar para ler_sensores_periodico
typedef struct {
    float *vs;
    float *va;
    int socket;
    struct sockaddr_in endereco_destino;
} args_sensores;

// ------------------------------------------


int cria_socket_local(void) {
    int socket_local;       /* Socket usado na comunicacao */

    socket_local = socket( PF_INET, SOCK_DGRAM, 0);
    if (socket_local < 0) {
        perror("socket");
        return -1;
    }
    return socket_local;
}

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino) {
    struct sockaddr_in servidor;    /* Endereco do servidor incluindo ip e porta */
    struct hostent *dest_internet;  /* Endereco destino em formato proprio       */
    struct in_addr dest_ip;     /* Endereco destino em formato ip numerico   */

    if (inet_aton ( destino, &dest_ip ))
        dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
    else
        dest_internet = gethostbyname(destino);

    if (dest_internet == NULL) {
        fprintf(stderr,"Endereco de rede invalido\n");
        exit(FALHA);
    }

    memset((char *) &servidor, 0, sizeof(servidor));
    memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(porta_destino);

    return servidor;
}

void envia_mensagem(int socket, struct sockaddr_in endereco_destino, char *mensagem) {
    /* Envia msg ao servidor */
    int endereco_size = sizeof(endereco_destino);
    struct sockaddr* endereco_pointer = (struct sockaddr*) &endereco_destino;
    int err = sendto(socket, mensagem, strlen(mensagem)+1,
                     0, endereco_pointer, endereco_size);
    if (err < 0) {
        perror("sendto");
        return;
    }
}

int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER) {
    int bytes_recebidos;   /* Numero de bytes recebidos */

    /* Espera pela msg de resposta do servidor */
    bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
    if (bytes_recebidos < 0)
    {
        perror("recvfrom");
    }

    return bytes_recebidos;
}

// sta30.0 -> 30.0
float extrair_num (char *s,int k){
    int n = strlen(s);
    char sub_s[1000];

    for (int i = k; i < n; i++) {
        sub_s[i-k] = s[i];
    }

    float x = atof(sub_s);
    return x;
}

// Lê um dos sensores baseado na requisição:
// sta0 -> ta : Temperatura do Ar do Ambiente
// st-0 -> t  : Temperatura da água no interior
// sti0 -> ti : Temperatura do água que entra no recipiente
// sto0 -> no : Fluxo de água de saída do recipiente
// sh-0 -> h  : Altura da coluna de água
float ler_sensor(int socket, struct sockaddr_in endereco_destino, char* requisicao) {
    char msg_recebida[1000];
    int nrec;

    envia_mensagem(socket,endereco_destino, requisicao);
    nrec = recebe_mensagem(socket,msg_recebida,1000);
    msg_recebida[nrec] = '\0';
    return extrair_num(msg_recebida,3);
}

// Lê todos os 5 sensores disponíveis no sistema.
void ler_sensores(float *vs, int socket, struct sockaddr_in endereco_destino){
    float ta = ler_sensor(socket, endereco_destino, "sta0");
    float t = ler_sensor(socket, endereco_destino, "st-0");
    float ti = ler_sensor(socket, endereco_destino, "sti0");
    float no = ler_sensor(socket, endereco_destino, "sto0");
    float h = ler_sensor(socket, endereco_destino, "sh-0");

    vs[0] = ta;
    vs[1] = t;
    vs[2] = ti;
    vs[3] = no;
    vs[4] = h;
}

//=======================================================================
/*                          THREAD ALARME                              */

void alarme(int state){

    if(state == 1) {
        printf("\nBIP!!!\n");
    }

}

//=======================================================================

//=======================================================================
/*                          THREAD TEMP_ALARME                        */

int temp_alarme(float *vs){
    if(vs[1] >= 50.0) {
        return 1;
    }
    else {
        return 0;
    }
}

void *temp_alarme_periodico(void *args){
    float *vs = (float*) args;
    struct timespec t;
    t.tv_sec = 1;
    long int periodo = 50000000;//0,05s
    clock_gettime(CLOCK_MONOTONIC,&t);

    while(1){
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        temp_alarme(vs);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}


void *alarme_periodico(void *args){

    float *vs = (float*) args;
    struct timespec t;
    t.tv_sec = 1;
    long int periodo = 4000000000;//0,5s
    clock_gettime(CLOCK_MONOTONIC,&t);

    while(1){
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        alarme(temp_alarme(vs));
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

//=======================================================================


//=======================================================================
//                      THREAD ARMAZENAR

void armanezar_temp_nv(float *vs){
    FILE *arq_hist;

    arq_hist = fopen(HISTORICO_ARQUIVO,"a");
    if(arq_hist == NULL) {
        printf("Erro ao abrir o arquivo");
    }
    fprintf(arq_hist,"T,%f\n",vs[0]);
    fprintf(arq_hist,"N,%f\n",vs[4]);

    fclose(arq_hist);
}

void *armazenar_temp_nv_periodico(void *args){

    float *vs = (float*) args;
    struct timespec t;
    t.tv_sec = 1;
    long int periodo = 400000000;
    clock_gettime(CLOCK_MONOTONIC,&t);

    while(1){
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        armanezar_temp_nv(vs);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }

    }
}
// =======================================================================

// co-rotina periódica para ler sensores de maneira periódica
// args: float *vs, int socket, struct sockaddr_in endereco_destino
void *ler_sensores_periodico(void *args) {

    struct timespec t;
    args_sensores* parametros =  args;
    float *vs = parametros->vs;
    int socket = parametros->socket;
    struct sockaddr_in endereco_destino = parametros->endereco_destino;

    // float *vs, int socket, struct sockaddr_in endereco_destino
    t.tv_sec = 1;
    long int periodo = 500000000; //0.5s
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        ler_sensores(vs, socket, endereco_destino);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }

}

// Imprime os valores dos sensores
void imprimir_valores(float *vs) {
    printf("== SENSORES\n");
    printf("Ta: \t%.2f\n ", vs[0]);
    printf("T: \t%.2f\n ", vs[1]);
    printf("Ti: \t%.2f\n ", vs[2]);
    printf("No: \t%.2f\n ", vs[3]);
    printf("H: \t%.2f\n ", vs[4]);

}

// co-rotina periódica para imprimir valores
void *imprimir_valores_periodico(void *arg) {
    // arg: float vs
    float *vs = (float*) arg;
    struct timespec t;
    t.tv_sec = 1;
    long int periodo = 2000000000; //1s
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        imprimir_valores(vs);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

//================================================================
/*                           THREAD TELA                        */

void tela_temp(float *vs){

    initscr();
    cbreak();

    while(1){

      pthread_mutex_lock(&lock);

        mvprintw(0,12,"+======================+");
        mvprintw(1,12,"|      SENSORES        |");
        mvprintw(2,12,"|======================|");

        mvprintw(3,12,"| Nivel: %05.2f (m)     |",vs[4]);
        mvprintw(4,12,"| Temp.: %05.2f (C)     |",vs[1]);

        mvprintw(5,12,"+======================+");

        refresh();


    }

    getch();
    endwin();
}

void *tela_periodico(void *arg) {
    // arg: float vs
    float *vs = (float*) arg;
    struct timespec t;
    t.tv_sec = 1;
    long int periodo = 500000000; //1s
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        tela_temp(vs);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}
//================================================================
//================================================================
/*             thread para controlar o nivel                    */
void envia_servidor_baixo_nivel(void *args){

  char msg_enviada_ni[1000];
  char msg_enviada_nf[1000];

  
  args_sensores* parametros =  args;
  float *vs = (float*) parametros->vs;
  float *va = (float*) parametros->va;
  int socket = parametros->socket;
  struct sockaddr_in endereco_destino = parametros->endereco_destino;

  printf("a\n");
        
  if(vs[4] > (float)3.00){

      va[0] -= 0.5;//ni
      va[1] += 0.1;//nf
    
      
      sprintf(msg_enviada_ni,"ani%lf",va[0]);   
      envia_mensagem(socket, endereco_destino, msg_enviada_ni);

      sprintf(msg_enviada_nf,"anf%lf",va[1]);   
      envia_mensagem(socket, endereco_destino, msg_enviada_nf);

  }
 
}

void *enviar_servidor_nivel_baixo_periodico(void *args) {
    // arg: float vs
    struct timespec t;
    t.tv_sec = 1;
    long int periodo =  700000000; //1s
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        envia_servidor_baixo_nivel(args);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}


//----------------------------------------------------------------------
void envia_servidor_cima_nivel(void *args){

  char msg_enviada_ni[1000];
  char msg_enviada_nf[1000];


  args_sensores* parametros =  args;
  float *vs = (float*) parametros->vs;
  float *va = (float*) parametros->va;
  int socket = parametros->socket;
  struct sockaddr_in endereco_destino = parametros->endereco_destino;
  
  printf("b\n");
        
  if(vs[4] < (float)3.00){
    
      va[0] += 0.1;//ni 
      va[1] -= 0.5;//nf
      
      sprintf(msg_enviada_ni,"ani%lf",va[0]);   
      envia_mensagem(socket, endereco_destino, msg_enviada_ni);
  
      sprintf(msg_enviada_nf,"anf%lf",va[1]);   
      envia_mensagem(socket, endereco_destino, msg_enviada_nf);
      
  }
  
}

void *enviar_servidor_nivel_cima_periodico(void *args) {
    // arg: float vs
    struct timespec t;
    t.tv_sec = 1;
    long int periodo =  700000000; //1s
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (1) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&t,NULL);
        envia_servidor_cima_nivel(args);
        t.tv_nsec += periodo;

        while(t.tv_nsec >= NSEC_PER_SEC){
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
    }
}

//================================================================
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: controlemanual <endereco> <porta>\n");
        fprintf(stderr, "<endereco> eh o endereco IP da caldeira\n");
        fprintf(stderr, "<porta> eh o numero da porta UDP da caldeira\n");
        fprintf(stderr, "Exemplo de uso:\n");
        fprintf(stderr, "   controlemanual localhost 12345\n");
        exit(FALHA);
    }

    // extração de parâmetros de argv
    int porta_destino = atoi(argv[2]);
    int socket_local = cria_socket_local();
    struct sockaddr_in endereco_destino = cria_endereco_destino(
        argv[1],
        porta_destino
        );

    // empacotamento de argumentos para a thread ler_sensores_periodico
    args_sensores args;
    args.vs = VS;
    args.va = VA;
    args.socket = socket_local;
    args.endereco_destino = endereco_destino;

    ler_sensores(VS, socket_local, endereco_destino);
    pthread_t threads[NUM_THREADS];
    pthread_create(&threads[0], NULL, ler_sensores_periodico,     (void *) &args);
    pthread_create(&threads[1], NULL, temp_alarme_periodico,      (void *) VS);
    pthread_create(&threads[2], NULL, alarme_periodico,           (void *) VS);
    pthread_create(&threads[3], NULL, armazenar_temp_nv_periodico,(void* ) VS);

    //pthread_create(&threads[4], NULL, imprimir_valores_periodico, (void *) VS);
    //pthread_create(&threads[4], NULL, tela_periodico, (void *) VS);
    pthread_create(&threads[4], NULL, enviar_servidor_nivel_baixo_periodico, (void *) &args);
    pthread_create(&threads[5], NULL, enviar_servidor_nivel_cima_periodico, (void *) &args);
    pthread_exit(NULL);

    return 0;

}
