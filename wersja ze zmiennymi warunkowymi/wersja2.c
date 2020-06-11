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
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t hairdresser_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t currClient_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t waitingRoom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t armchair = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hairdresser_mut = PTHREAD_MUTEX_INITIALIZER;

int spots = 7; //ilość miejsc w poczekalni
int freeSpots = 7; //ilość wolnych miejsc
int resignedClients = 0; // liczba klientów, którzy zrezygnowali z wizyty
int clients = 10;
int actualClient = 0;
int currentClient = -1;
int passedClients = 0;
bool debug = false;
int haircuttingTime = 3;
int clientsTime = 2;

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
    freeSpots++;
}

void wait_random_time(int max){
    int time = rand() % (max + 1);
    usleep(time * 1000);
}

void *newClient(void *num){ //funkcja rozpoczynająca 'wizytę' klienta
    int nr_client = *(int *)num;
    pthread_mutex_lock(&waitingRoom); //blokujemy poczekalnię

    if(freeSpots){ //jeśli są wolnej miejsca
        add_to_waiting_queue(nr_client); //dodajemy klienta do klientów czekających w poczekalni
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug) {
            printQueues();
        }
        //sem_post(&client);//daje sygnał fryzjerowi, że ktoś czeka w poczekalni //TODO nie wiem czy tego może nie dać przed odblokowaniem mutexu
        //pthread_mutex_lock(&hairdresser_mut);
        pthread_cond_signal(&client_cond);
        //pthread_mutex_unlock(&hairdresser_mut);
        //pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
        //pthread_mutex_lock(&waitingRoom); //tu prawdopodobnie powinien być inny mutex?
        while(nr_client != currentClient){
            pthread_cond_wait(&hairdresser_cond, &waitingRoom);
        }
        //printf("Przeszedł %d\n\n",nr_client);
        pthread_mutex_unlock(&waitingRoom);
    }
    else{
        passedClients++;
        add_to_resigned_queue(nr_client); //jeśli brak wolnych miejsc to dodajemy go do kolejki klientów którzy zrezygnowali
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug){
            printQueues();
        }
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
    }
    pthread_exit(0);
}

void *hairdresserRoom(){
    //sem_post(&hairdresser);

    while(passedClients != clients){
        pthread_mutex_lock(&waitingRoom);
        //sem_wait(&client);//tutaj śpi, czyli czeka na klienta
        if(freeSpots == spots)	{
            currentClient = -1;
            pthread_cond_wait(&client_cond, &waitingRoom); //czeka na klienta
        }
        //pthread_mutex_lock(&hairdresser_mut);
        currentClient = waiting->client_number;
        pthread_cond_broadcast(&hairdresser_cond);
        //pthread_mutex_unlock(&hairdresser_mut);
        pthread_mutex_unlock(&waitingRoom);
        delete_from_waiting_queue(currentClient);
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug) {
            printQueues();
        }
        pthread_mutex_lock(&armchair);//blokuje fotel u fryzjera ?
        passedClients++;
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni
        wait_random_time(haircuttingTime);
        pthread_mutex_unlock(&armchair); //odblokowanie fotela
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
    srand(time(NULL));
    int choice;
    static struct option long_options[] = {
            {"debug", no_argument, NULL, 'd'},
            {"clients", required_argument, NULL, 'n'},
            {"spots", required_argument, NULL, 's'},
            {"haircuttingTime", required_argument, NULL, 'h'},
            {"clientsTime", required_argument, NULL, 'c'},
    };
    while((choice = getopt_long_only(argc,argv,":dnshc", long_options, NULL)) != -1){ //checking and setting options from user's choice
        switch(choice){
            case 'd':
                debug = true;
                break;
                /*case ':':
                    puts("Missing an operand");
                    syslog(LOG_ERR, "Missing an operand");
                    exit(EXIT_FAILURE);*/
            case 'n':
                if (atoi(optarg) <= 0) {
                    puts("Invalid number of clients");
                    exit(EXIT_FAILURE);
                } else
                    clients = atoi(optarg);
                break;
            case 's':
                if (atoi(optarg) <= 0) {
                    puts("Invalid number of spots");
                    exit(EXIT_FAILURE);
                } else
                    spots = atoi(optarg);
                break;
            case 'h':
                if (atoi(optarg) <= 0) {
                    puts("Invalid time for haircutting");
                    exit(EXIT_FAILURE);
                } else
                    haircuttingTime = atoi(optarg);
                break;
            case 'c':
                if (atoi(optarg) <= 0) {
                    puts("Invalid time for clients");
                    exit(EXIT_FAILURE);
                } else
                    clientsTime = atoi(optarg);
                break;
            default:
                puts("No such option");
                exit(EXIT_FAILURE);
        }
    }
    printf("clients: %d\n", clients);
    printf("spots: %d\n", spots);
    printf("haircuttingTime: %d\n", haircuttingTime);
    printf("clientsTime: %d\n", clientsTime);

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
        wait_random_time(clientsTime);
        arg[i] = i;
        iret = pthread_create(&threads[i], NULL, newClient, (void*)&arg[i]);
        //iret = pthread_create(&threads[i], NULL, printString, (void*)&arg[i]);
        if (iret) {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret);
            exit(EXIT_FAILURE);
        }
    }
    for(int i = 0; i < clients; i++){
        pthread_join(threads[i], NULL);
    }
    pthread_join(haird, NULL);

    sem_destroy(&client);
    sem_destroy(&hairdresser);
    pthread_cond_destroy(&client_cond);
    pthread_cond_destroy(&hairdresser_cond);
    pthread_cond_destroy(&currClient_cond);
    pthread_mutex_destroy(&waitingRoom);
    pthread_mutex_destroy(&armchair);
    pthread_mutex_destroy(&hairdresser_mut);
    clean_queue();
    exit(EXIT_SUCCESS);
}