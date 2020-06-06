#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>
#include <stdbool.h>
#include <getopt.h>

typedef struct Queue {
    int client_number;
    struct Queue* next_client;
}Queue; //FIFO dla klientów

Queue *waiting = NULL; //kolejka dla czekających klientów (poczekalnia)
Queue *resigned = NULL; //Lista klientów, którzy zrezygnowali z usługi

Queue *last_waiting = NULL; //trzyma ostatnią osobę która czeka, żeby łatwo dodać na koniec kolejki
Queue *last_resigned = NULL; //nie wiadomo czy będzie potrzebne

sem_t client;
sem_t hairdresser;
pthread_mutex_t waitingRoom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t armchair = PTHREAD_MUTEX_INITIALIZER;

int spots = 7; //ilość miejsc w poczekalni
int freeSpots = 7; //ilość wolnych miejsc
int resignedClients = 0; // liczba klientów, którzy zrezygnowali z wizyty
int clients = 10;
int actualClient = 0;
int currentClient = -1;
int passedClients = 0;
bool debug = false;

void printQueues(){ //wypisywanie kolejek
    if(waiting == NULL){ //wypisywanie kolejki oczekujących
        puts("Waiting queue is empty");
    }
    else{
        printf("Waiting queue: ");
        Queue *curr = waiting;
        printf("%d, ", curr->client_number);
        while(curr->next_client != NULL){
            curr = curr->next_client;
            printf("%d, ", curr->client_number);
        }
        printf("\n");
    }
    if(resigned == NULL){ //wypisywanie kolejki klientów, którzy zrezygnowali
        puts("Resigned queue is empty");
    }
    else{
        printf("Resigned queue: ");
        Queue *curr = resigned;
        printf("%d, ", curr->client_number);
        while(curr->next_client != NULL){
            curr = curr->next_client;
            printf("%d, ", curr->client_number);
        }
        printf("\n");
    }
}

void add_to_waiting_queue(int number){ //dodawanie do kolejki klientów oczekujących; brak wywołania jeśli jest pełna
    Queue *new = (Queue*)malloc(sizeof(Queue));
    new->client_number = number;
    new->next_client = NULL;
    if (last_waiting == NULL) { //pusta kolejka
        waiting = new; //ustawienie pierwszego klienta
    } else { //są jeszcze wolne miejsca
        last_waiting->next_client = new; //dodanie klienta do kolejki
    }
    last_waiting = new; //ustawienie ostatniego klienta
    freeSpots--; //miejsce jest zajęte
}

void add_to_resigned_queue(int number){ //dodawanie do kolejki klientów, którzy zrezygnowali
    Queue *new = (Queue*)malloc(sizeof(Queue));
    new->client_number = number;
    new->next_client = NULL;
    if( last_resigned == NULL){ //pusta kolejka
        resigned = new; //ustawianie pierwszego elementu
    }
    else{
        last_resigned->next_client = new; //dodawanie klienta do kolejki
    }
    last_resigned = new; //ustawienie ostatniego elementu kolejki
    resignedClients++;
}

void delete_from_waiting_queue(){ //usuwanie pierwszego klienta z kolejki oczekujących
    if(waiting->next_client == NULL){ //jeśli był sam to zwalniamy pamięć
        Queue* x = waiting;
        free(x);
        waiting = NULL;
        last_waiting = NULL;
    }
    else{ //jeśli nie to usuwamy pierwszego klienta z kolejki
        Queue* first = waiting;
        waiting = first->next_client;
        free(first);
    }
}

void *newClient(void *num){ //funkcja rozpoczynająca 'wizytę' klienta
    int nr_client = *(int *)num;
    pthread_mutex_lock(&waitingRoom); //blokujemy poczekalnię

    if(freeSpots){ //jeśli są wolnej miejsca
        add_to_waiting_queue(nr_client); //dodajemy klienta do klientów czekających w poczekalni
        if(debug == true) {
            printQueues();
        }
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
        sem_post(&client);//daje sygnał fryzjerowi, że ktoś czeka w poczekalni
        sem_wait(&hairdresser); //czeka na zwolnienie się fryzjera ?
        currentClient = nr_client;
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
    }
    else{
        passedClients++;
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
        add_to_resigned_queue(nr_client); //jeśli brak wolnych miejsc to dodajemy go do kolejki klientów którzy zrezygnowali
        if(debug == true){ //TODO nie wiem czy nie trzeba na czas wypisywania dać jakiegoś mutexu, bo przy tym może też się pojawić błąd, może przesunęła bym za wypisanie odblokowanie poczekalni
            printQueues();
        }
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
    }
    pthread_exit(0);
}

void *hairdresserRoom(){
    sem_post(&hairdresser);
    while(passedClients != clients){
        sem_wait(&client);//tutaj śpi, czyli czeka na klienta
        freeSpots++;//
        delete_from_waiting_queue();
        //obsługa pierwszego w kolejce wątku
        if(debug == true) {
            printQueues();
        }
        pthread_mutex_lock(&waitingRoom);//blokujemy poczekalnię, bo sprawdza czy jest klient
        pthread_mutex_lock(&armchair);//blokuje fotel u fryzjera ?
        passedClients++;
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
        //sleep(2);
        pthread_mutex_unlock(&armchair); //odblokowanie fotela
        sem_post(&hairdresser);
    }
}

void clean_queue(){ //usuwanie pierwszego klienta z kolejki oczekujących
    if(resigned == NULL){
        return;
    }
    while(resigned->next_client != NULL){ //jeśli był sam to zwalniamy pamięć
        Queue* x = resigned;
        resigned = resigned->next_client;
        free(x);
    }
    free(resigned);
}


int main(int argc, char *argv[]) {
    int choice;
    static struct option long_options[] = {
            {"debug", optional_argument, NULL, 'd'},
    };
    while((choice = getopt_long_only(argc,argv,"d;", long_options, NULL)) != -1){ //checking and setting options from user's choice
        switch(choice){
            case 'd':
                debug = true;
                break;
                /*case ':':
                    puts("Missing an operand");
                    syslog(LOG_ERR, "Missing an operand");
                    exit(EXIT_FAILURE);*/
            default:
                puts("No such option");
                exit(EXIT_FAILURE);
        }
    }
    sem_init(&client,0,0);
    sem_init(&hairdresser,0,0);
    // drukarka z mutexem
    pthread_t threads[clients];
    pthread_t haird;
    int iret;
    int iret2;
    iret2 = pthread_create(&haird, NULL, hairdresserRoom , NULL);
    if (iret2) {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret2);
        exit(EXIT_FAILURE);
    }
    int arg[clients];
    for(int i = 0; i < clients; i++) {
        arg[i] = i;
        iret = pthread_create(&threads[i], NULL, newClient, (void*)&arg[i]);
        //iret = pthread_create(&threads[i], NULL, printString, (void*)&arg[i]);
        if (iret) {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret);
            exit(EXIT_FAILURE);
        }
        //sleep(3);
    }
    for(int i = 0; i < clients; i++){
        pthread_join(threads[i], NULL);
    }
    pthread_join(haird, NULL);

    sem_destroy(&client);
    sem_destroy(&hairdresser);
    pthread_mutex_destroy(&waitingRoom);
    pthread_mutex_destroy(&armchair);
    clean_queue();
    exit(EXIT_SUCCESS);
}