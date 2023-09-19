#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>

#define SIZE_MEX sizeof(char)*1023

pthread_cond_t condition[3];

int shared_memory_fd;

int ID_server;
int socket_fd;

static void segnale_server(){
    fprintf(stdout, "SERVER %d EXITING\n", ID_server);
    sem_unlink("/sem_Supervisor");
    shm_unlink("/shared_memory");
    sem_unlink("/sem_can_copy");
    close(shared_memory_fd);
    close(socket_fd);
    exit(0);
}

struct inf_client{
    fd_set *gest_client;
    int *gest_max_fd;
    pthread_mutex_t *mutex_client;
    pthread_mutex_t *mutex_gest_client;
    pthread_cond_t *condition;
};

void* gestione_client(void* info){
    fd_set* gest_client = ((struct inf_client*) info) -> gest_client;
    int* gest_max_fd = ((struct inf_client*) info) -> gest_max_fd;
    pthread_mutex_t* mutex_client = ((struct inf_client*) info) -> mutex_client;
    pthread_mutex_t* mutex_gest_client = ((struct inf_client*) info) -> mutex_gest_client;
    pthread_cond_t* condition = ((struct inf_client*) info) -> condition;
    int byte_read;
    long int *buffer = (long int*) malloc(sizeof(long int));
    long int client_id;
    long int tempo_migliore;
    long int tempo_attuale;
    struct timespec inizio;
    char *temp_stringa;

    sem_t *sem_Supervisor;
    sem_Supervisor = sem_open("/sem_Supervisor", 0666);

    if(sem_Supervisor == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    
    if((shared_memory_fd = shm_open("/shared_memory", O_RDWR, 0666)) < 0){
        perror("SHM_OPEN");
        exit (1);
    }

    char *shared_data = (char*) mmap(NULL, SIZE_MEX, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);

    if(!shared_data){
        perror("MMAP");
        exit (1);
    }

    sem_t *sem_can_copy;
    sem_can_copy = sem_open("/sem_can_copy", 0666);
    if(sem_can_copy == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
     
    while(1){
        temp_stringa = (char*) malloc(sizeof(shared_data));

        pthread_mutex_lock(mutex_gest_client);
        
        pthread_cond_wait(condition, mutex_gest_client);
        fd_set temp_gest_client = *gest_client;
        int temp_max_fd = *gest_max_fd;

        pthread_mutex_unlock(mutex_gest_client);
        
        if(select(*(gest_max_fd) + 1, gest_client, NULL, NULL, NULL) < 0){
            perror("SELECT");
            exit(1);
        }

        for(int temp_sock = 0; temp_sock <= *(gest_max_fd); temp_sock++){
            tempo_migliore = -1;
            tempo_attuale = -1;
            if(FD_ISSET(temp_sock, gest_client)){
                while((byte_read = read(temp_sock, buffer, sizeof(buffer)))>0){ //leggo i messaggi e calcolo il secret.
                    client_id = be64toh(*buffer); 
                    clock_gettime(CLOCK_MONOTONIC, &inizio);
                    if(tempo_attuale == -1){
                        tempo_attuale = inizio.tv_sec * 1000 + (int)inizio.tv_nsec/1000000;
                    }else{
                        tempo_attuale = (inizio.tv_sec * 1000 + (int)inizio.tv_nsec/1000000) - tempo_attuale;
                        if(tempo_migliore == -1 || tempo_attuale < tempo_migliore){
                            tempo_migliore = tempo_attuale;
                        }
                        tempo_attuale = inizio.tv_sec * 1000 + (int)inizio.tv_nsec/1000000;
                    }
                    fprintf(stdout, "SERVER %d INCOMING FROM %lx @ %ld \n", ID_server, client_id, tempo_attuale);
                }
                if(byte_read < 0){
                    perror("READ");
                    exit (1);
                }
                fprintf(stdout,  "SERVER %d CLOSING %lx ESTIMATE %ld \n", ID_server, client_id, tempo_migliore);
                sprintf(temp_stringa, "%ld!%ld!%d!", tempo_migliore, client_id, ID_server);
                
                pthread_mutex_lock(mutex_client);
                sem_wait(sem_can_copy); 
                memcpy(shared_data, temp_stringa, SIZE_MEX); 
                sem_post(sem_Supervisor);
                pthread_mutex_lock (mutex_gest_client);
                FD_CLR(temp_sock, gest_client);
                if(temp_sock == *gest_max_fd){
                    while(FD_ISSET(*gest_max_fd, gest_client) == 0){
                        (*gest_max_fd)--;
                    }
                }
                pthread_mutex_unlock (mutex_gest_client);
                close(temp_sock);
                pthread_mutex_unlock(mutex_client);
            }
        }
    }
}

void* accettazione_client(){
    struct sockaddr_in sock_server;
    pthread_t *tid_gestione = (pthread_t*) malloc(sizeof(pthread_t) * 3);
    
    sock_server.sin_family=AF_INET;
    sock_server.sin_addr.s_addr=htonl(INADDR_ANY);
    sock_server.sin_port=htons(9000+ID_server);

    if((socket_fd=socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("SOCKET");
        exit (1);
    }

    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    if(bind(socket_fd,(struct sockaddr *)&sock_server,sizeof(sock_server)) < 0){
        perror("BIND");
        exit (1);
    }

    if(listen(socket_fd, 3) < 0){
        perror("LISTEN");
        exit (1);
    }

    fd_set *sock_clienti= (fd_set*) malloc(sizeof(fd_set)*3);
    pthread_mutex_t *mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t)*3);
    pthread_mutex_t all_mutex;
    pthread_mutex_init(&all_mutex, NULL);
    
    int *max_fd = (int*)calloc(3,sizeof(int));

    for(int i = 0; i < 3; i++){
        FD_ZERO(&sock_clienti[i]);
        pthread_mutex_init(&mutex[i], NULL);

        struct inf_client *info = (struct inf_client*) malloc(sizeof(struct inf_client));
        info->gest_client = &(sock_clienti[i]);
        info->mutex_client = &(all_mutex);
        info->mutex_gest_client = &(mutex[i]);
        info->gest_max_fd = &(max_fd[i]);
        info->condition = &(condition[i]);
        
        pthread_create (&(tid_gestione[i]), NULL, gestione_client, info);
        pthread_detach (tid_gestione[i]);

    }

    int thread_worker = 0; 
    int client_accettato_fd; 

    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        if((client_accettato_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len)) < 0){
            perror("ACCEPT");
            exit (1);
        }

        FD_SET(client_accettato_fd, &sock_clienti[thread_worker]);
        if(client_accettato_fd > max_fd[thread_worker]){
            max_fd[thread_worker] = client_accettato_fd;
        }
        pthread_cond_signal(&condition[thread_worker]);  

        thread_worker = (thread_worker + 1) % 3;
    }
}


int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stdout, "Errore nel numero di argomenti: inserire 1 numero positivo \n");
        return 1;
    }

    ID_server = atoi(argv[1]);
    fprintf(stdout, "SERVER %d ACTIVE\n", ID_server);

    struct sigaction sign_S; 
    sign_S.sa_handler = segnale_server;
    sign_S.sa_flags = SA_RESTART;
    sigemptyset(&sign_S.sa_mask);

    if(sigaction(SIGUSR1, &sign_S, NULL) < 0){
        perror("SIGACTION");
        return 1;
    }

    for(int i = 0; i < 3; i++){
        pthread_cond_init(&condition[i], NULL);
    }

    pthread_t tid_accettazione;
    pthread_create (&tid_accettazione, NULL, accettazione_client, NULL);
    pthread_join   (tid_accettazione, NULL);

    return 0;
}
