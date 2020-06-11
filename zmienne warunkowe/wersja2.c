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
}Queue; //element kolejki

Queue *waiting = NULL; //kolejka dla czekających klientów (poczekalnia)
Queue *resigned = NULL; //Lista klientów, którzy zrezygnowali z usługi

Queue *last_waiting = NULL; //trzyma ostatnią osobę która czeka, żeby łatwo dodać na koniec kolejki
Queue *last_resigned = NULL; //trzyma ostatniego klienta, który zrezygnował, żeby łatwo dodać na koniec kolejki

sem_t client;
sem_t hairdresser;

pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t hairdresser_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t waitingRoom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t armchair = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hairdresser_mut = PTHREAD_MUTEX_INITIALIZER;

int spots = 7; //ilość miejsc w poczekalni
int freeSpots = 7; //ilość wolnych miejsc
int resignedClients = 0; // liczba klientów, którzy zrezygnowali z wizyty
int clients = 10; //liczba klientów
int currentClient = -1; //numer klienta aktualnie zajmującego fotel (dla mutexu poczekalni)
int passedClients = 0; //suma liczby obsłużonych klientów i klientów którzy zrezygnowali
bool debug = false; //czy wypisuje kolejki
int haircuttingTime = 3; //maksymalny cas ścinania klienta w ms
int clientsTime = 2; //maksymalny czas między tworzeniem wątków kliwntów w ms
int currIn = -1; //numer klienta aktualnie zajmującego fotel (dla mutexu fryzjera)

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
    } else {
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

void delete_from_waiting_queue(int cNumber){ //usuwanie klienta z kolejki oczekujących
    if((waiting->next_client == NULL) && (waiting->client_number == cNumber)){
        Queue* x = waiting;
        free(x);
        waiting = NULL;
        last_waiting = NULL;
    }
    else if(waiting->client_number == cNumber){
        Queue* first = waiting;
        waiting = first->next_client;
        free(first);
    }
    else{
        Queue* x = waiting;
        while(x->next_client->client_number != cNumber){
            x = x->next_client;
        }
        Queue* tmp = x->next_client;
        x->next_client = tmp->next_client;
        if(tmp = last_waiting){
            last_waiting = x;
        }
        free(tmp);
    }
    freeSpots++;
}

void wait_random_time(int max){ //czekanie losowego czasu
    int time = rand() % (max + 1);
    usleep(time * 1000);
}

void *newClient(void *num){ //funkcja rozpoczynająca 'wizytę' klienta
    int nr_client = *(int *)num;
    pthread_mutex_lock(&waitingRoom); //blokujemy poczekalnię aby klient mógł sprawdzić czy może do niej wejść
    if(freeSpots){ //jeśli są wolnej miejsca
        add_to_waiting_queue(nr_client); //dodajemy klienta do klientów czekających w poczekalni i zajmuje on miejsce
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug) {
            printQueues();
        }
        pthread_cond_signal(&client_cond);//daje sygnał fryzjerowi, że przyszedł nowy klient
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni ponieważ klient już zajął miejsce
        pthread_mutex_lock(&hairdresser_mut); //ticket lock wpuszczający klientów, kiedy to oni zajęli miejsce u fryzjera
        while(nr_client != currIn){
            pthread_cond_wait(&hairdresser_cond, &hairdresser_mut);//czekanie na sygnał, że ktoś z kolejki zajął fotel
        }
        pthread_mutex_unlock(&hairdresser_mut);//odblokowanie mutexu zablokowanego przez wchodzący do fryzjera wątek
    }
    else{
        passedClients++;
        add_to_resigned_queue(nr_client); //jeśli brak wolnych miejsc to dodajemy go do kolejki klientów którzy zrezygnowali
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug){
            printQueues();
        }
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni ponieważ klient zrezygnował i wyszedł
    }
    pthread_exit(0);
}

void *hairdresserRoom(){
    while(passedClients != clients){ //dopóki wszyscy nie zostali obsłużeni lub nie zrezygnowali
        pthread_mutex_lock(&waitingRoom); //blokowanie poczekalni aby fryzjer sprawdził czy ktoś czeka w poczekalni
        if(freeSpots == spots){ //jeśli poczekalnia jest pusta
            currentClient = -1; //nikogo nie ma na fotelu
            pthread_cond_wait(&client_cond, &waitingRoom); //fryzjer śpi i czeka na klienta
        }
        currentClient = waiting->client_number; //ustawienie numeru aktualnie obsługiwanego klienta dla mutexu poczekalni
        delete_from_waiting_queue(currentClient); //usunięcie klienta z kolejki oczekujących
        printf("Res:%d WRomm: %d/%d [in: %d]\n", resignedClients, spots - freeSpots, spots,  currentClient);
        if(debug) {
            printQueues();
        }
        pthread_mutex_lock(&hairdresser_mut); //zablokowanie fryzjera żeby rozesłał broadcast do wątków oczekujących w ticket lock, że jeden z nich może go opuścić
        currIn = currentClient; //ustawienie numeru aktualnie obsługiwanego klienta dla mutexu fryzjera
        pthread_cond_broadcast(&hairdresser_cond);
        pthread_mutex_unlock(&hairdresser_mut);
        pthread_mutex_lock(&armchair);//blokuje fotel u fryzjera
        passedClients++;
        pthread_mutex_unlock(&waitingRoom); //odblokowanie poczekalni ponieważ klient zajął już fotel
        wait_random_time(haircuttingTime); //obsługa klienta
        pthread_mutex_unlock(&armchair); //odblokowanie fotela po zakończeniu strzyżenia
    }
}

void clean_queue(){ //czyszczenie kolejki klientów, którzy zrezygnowali
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
    while((choice = getopt_long_only(argc,argv,":dnshc", long_options, NULL)) != -1){ //sprawdzanie opcji użytkownika
        switch(choice){
            case 'd':
                debug = true;
                break;
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
                freeSpots = atoi(optarg);
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
    pthread_t threads[clients];
    pthread_t haird;
    int iret;
    int iret2;
    iret2 = pthread_create(&haird, NULL, hairdresserRoom , NULL); //tworzenie wątku fryzjera
    if (iret2) {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret2);
        exit(EXIT_FAILURE);
    }
    int arg[clients];
    for(int i = 0; i < clients; i++) { //tworzenie wątków klientów
        wait_random_time(clientsTime); //odczekiwanie losowego czasu
        arg[i] = i; //przydzielenie numeru wątku
        iret = pthread_create(&threads[i], NULL, newClient, (void*)&arg[i]); //tworzenie wątku klienta
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
    pthread_mutex_destroy(&waitingRoom);
    pthread_mutex_destroy(&armchair);
    pthread_mutex_destroy(&hairdresser_mut);
    clean_queue(); //czyszczenie kolejki klientów, którzy zrezygnowali
    exit(EXIT_SUCCESS);
}