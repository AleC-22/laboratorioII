#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define SIZE_MEX sizeof(char)*1024

int count=0;
struct timeval tempo_segnale;
int secondi;

struct lista_processi{
    int pid;
    struct lista_processi *next;
};

struct lista_processi *head_processi=NULL;

struct lista_tabella{
    long int ID_client;
    int stima_secret;
    int numero_server; 
    struct lista_tabella *next;
};

struct lista_tabella *head = NULL;

static void funzione_SIGINT(int signum){
    count++;
    gettimeofday(&tempo_segnale, NULL);
    struct lista_tabella *temp = head;
    fprintf(stderr, "\n%20s | %20s | %20s \n", "ID Client", "Stima Secret", "Numero Server");
    while(temp != NULL){
        fprintf(stderr, "%20lx | %20d | %20d \n", temp->ID_client, temp->stima_secret, temp->numero_server);
        temp = temp->next;
    }
    if(count==1){
        secondi = tempo_segnale.tv_sec;
    } else {
        if(tempo_segnale.tv_sec - secondi <= 1){
            struct lista_tabella *temp = head;
            fprintf(stdout, "\n%20s | %20s | %20s \n", "ID Client", "Stima Secret", "Numero Server");
            while(temp != NULL){
                fprintf(stdout, "%20lx | %20d | %20d \n", temp->ID_client, temp->stima_secret, temp->numero_server);
                temp = temp->next;
            }
            fprintf(stdout, "SUPERVISOR EXITING\n");
            while(head_processi != NULL){
                kill(head_processi->pid, SIGUSR1);
                head_processi = head_processi->next;
            }
            while(head_processi != NULL){
                struct lista_processi *temp = head_processi;
                head_processi = head_processi->next;
                free(temp);
            }
            shm_unlink("/shared_memory");
            sem_unlink("/sem_Supervisor");
            while(head != NULL){
                struct lista_tabella *temp = head;
                head = head->next;
                free(temp);
            }
            exit(0);
        } else {
            secondi = tempo_segnale.tv_sec;
        }
    }
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stdout, "Errore nel numero di argomenti: inserire un numero positivo \n");
        return 1;
    }

    struct sigaction nuovo_segnale;
    nuovo_segnale.sa_handler = funzione_SIGINT; 
    nuovo_segnale.sa_flags = SA_RESTART;
    sigemptyset(&nuovo_segnale.sa_mask);
    
    if(sigaction(SIGINT, &nuovo_segnale, NULL) < 0){
        perror("SIGACTION");
        return 1;
    }

    int S = atoi(argv[1]); //numero totale server 

    if(S < 1){
        fprintf(stdout, "Errore nel numero dei Server S: inserire un numero positivo \n");
        return 1;
    }

    fprintf(stdout, "SUPERVISOR STARTING %d SERVER \n", S); 

    int shared_memory_fd;
    if ((shared_memory_fd=shm_open("/shared_memory", O_CREAT | O_RDWR, 0666)) < 0){
        perror("SHM_OPEN");
        return 1;
    }
    
    if(ftruncate(shared_memory_fd, SIZE_MEX) < 0){
        perror("FTRUNCATE");
        return 1;
    }

    char *shared_data = mmap(NULL, SIZE_MEX, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if(!shared_data){
        perror("MMAP");
        return 1;
    }

    close(shared_memory_fd);

    int pid;
    int pid_Supervisor = getpid();
    int ID = 0;
    char ID_string[5];
    
    for(int i=0; i<S; i++){
        if(getpid() == pid_Supervisor){
            ID++;
            if((pid = fork()) < 0){
                perror("FORK");
                return 1;
            }
            struct lista_processi *temp = malloc(sizeof(struct lista_processi));
            temp->pid = pid;
            temp->next = NULL;
            if(head_processi == NULL){
                head_processi = temp;
            } else {
                struct lista_processi *temp2 = head_processi;
                while(temp2->next != NULL){
                    temp2 = temp2->next;
                }
                temp2->next = temp;
            }
        }
    }

    
    if(getpid() != pid_Supervisor){
        sprintf(ID_string, "%d", ID);
        execl("./server", "server", ID_string, NULL);
        fprintf(stderr, "Errore nell'esecuzione di execl \n");
    } else { 
        // stringa del tipo "stima_secret!id_client!id_server"
        char stima_secretSM[64];
        char ID_clientSM[64];
        char ID_serverSM[64];
        char *copia_stringa = (char*) malloc(SIZE_MEX);
        
        sem_t *sem_Supervisor = sem_open("/sem_Supervisor", O_CREAT, 0666, 0);
        if(sem_Supervisor == SEM_FAILED){
            perror("SEM_OPEN");
            return 1;
        }
        sem_t *sem_can_copy = sem_open("/sem_can_copy", O_CREAT, 0666, 1);
        if(sem_can_copy == SEM_FAILED){
            perror("SEM_OPEN");
            return 1;
        }
        //sem_post(sem_can_copy);
        while(1){         
            sem_wait(sem_Supervisor);
            strcpy(copia_stringa, shared_data);
            //fprintf(stdout,"[SUPERVISOR] TEMP STRING: %s\n",copia_stringa);
            strcpy(stima_secretSM, strtok(copia_stringa, "!"));
            strcpy(ID_clientSM, strtok(NULL, "!"));
            strcpy(ID_serverSM, strtok(NULL, "!"));
            //fprintf(stdout, "[SUPERVISOR] %s \n", copia_stringa);
            long int ID_client= atol(ID_clientSM);
            int stima_secret = atoi(stima_secretSM);
            if(stima_secret != -1 && ID_client != 0){
                struct lista_tabella *new = (struct lista_tabella*) malloc(sizeof(struct lista_tabella));
            
                new->ID_client = ID_client;
                new->stima_secret = stima_secret;
                new->numero_server = 1;
                new->next = NULL;
                int flag;
                if(head == NULL){
                    head = new;
                } else {
                    struct lista_tabella *temp = head;
                    flag = 0;
                    while(temp != NULL){
                        if(temp->ID_client == new->ID_client){
                            flag = 1;
                            temp->numero_server++;
                            if(new->stima_secret!=-1 && temp->stima_secret > new->stima_secret){
                                temp->stima_secret = new->stima_secret;
                            }
                        }
                        temp = temp->next;
                    }
                    if(flag == 0){
                        new->next = head;
                        head = new;
                    }
                }
            }
            fprintf(stdout, "SUPERVISOR ESTIMATE %s FOR %lx FROM %s \n", stima_secretSM, ID_client, ID_serverSM);
            memset(shared_data, 0, SIZE_MEX);
            memset(stima_secretSM, 0, sizeof(long) + sizeof(int));
            memset(ID_clientSM, 0, sizeof(long) + sizeof(int));
            memset(ID_serverSM, 0, sizeof(long) + sizeof(int));
            sem_post(sem_can_copy); 
        }
    }
}
