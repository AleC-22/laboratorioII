#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char **argv){
    if(argc != 4){
        fprintf(stdout, "Errore nel numero di argomenti: inserire 3 numeri positivi \n");
        return 1;
    }

    int S = atoi(argv[1]); //numero di server totali
    int k = atoi(argv[2]); //numero di server a cui connettersi
    int m = atoi(argv[3]); //numero messaggi da inviare
    
    if(S < 1){
        fprintf(stdout, "Errore nel valore di S: inserire un numero positivo \n");
        return 1;
    }
    
    if(k < 0 || k > S){
        fprintf(stdout, "Errore nel valore di k: inserire un valore compreso tra 0 e il numero di Server \n"); 
        return 1;
    }
    
    if(m < 3*k){
        fprintf(stdout, "Errore nel valore di m: inserire un numero maggiore o uguale a 3 volte il numero di Server da connettersi \n");
        return 1;
    }
    
    srand(time(NULL));
    int secret = rand() % 3000 + 1;
    long int ID = rand();
    fprintf(stdout, "CLIENT %lx SECRET %d\n", ID, secret);
    
    int random_array[k];
    int random_k;
    int isPresent;
    int i=0;
    
    while(i < k){
        isPresent = 0;
        random_k = rand() % S + 1;

        for (int j = 0; j < i; j++){
            if(random_array[j] == random_k){
                isPresent = 1;
                break;
            }
        }
        
        if(isPresent == 0){
            random_array[i] = random_k;
            i++;
        }
    }
    
    int array_socket_fd[k];
    struct sockaddr_in address[k];
    int random_index;
    
    for (int i = 0; i < k; i++){
        if((array_socket_fd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            perror("SOCKET");
            return 1;
        }

        address[i].sin_family = AF_INET;
        address[i].sin_addr.s_addr = htonl(INADDR_ANY);
        address[i].sin_port = htons(9000+random_array[i]);

        if(connect(array_socket_fd[i], (struct sockaddr *)&address[i], sizeof(address[i])) < 0){
            perror("CONNECT");
            return 1;
        }

    }
    
    long int message = htobe64(ID);
    struct timespec wait_time;
    wait_time.tv_sec = secret / 1000;
    wait_time.tv_nsec = (secret % 1000) * 1000000;
    
    for (int i = 0; i < m; i++){
        random_index = rand() % k;
        if(write(array_socket_fd[random_index], &message, sizeof(message)) < 0){
            perror("WRITE");
            return 1;
        }
        nanosleep(&wait_time, NULL);
    }
    
    for(int i = 0; i < k;i++){
        close(array_socket_fd[i]);
    }

    fprintf(stdout, "CLIENT %lx DONE \n", ID);
    
    return 0;
}
