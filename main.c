#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct Queue {
    int client_number;
    Queue* next_client;
}Queue; //FIFO dla klientów

Queue *waiting = NULL; //kolejka dla czekających klientów (poczekalnia)
Queue *resigned = NULL; //Lista klientów, którzy zrezygnowali z usługi

Queue *last_waiting = NULL;
Queue *last_resigned = NULL;

int spots = 7; //ilość miejsc w poczekalni
int freeSpots = 7; //ilość wolnych miejsc

void printQueues(){ //wypisywanie kolejek
    if(waiting == NULL){ //wypisywanie kolejki oczekujących
        puts("Waiting queue is empty");
    }
    else{
        printf("Waiting queue: ")
        Queue *curr = waiting;
        printf("%d, ", curr->client_number);
        while(curr->next_client != NULL){
            curr = curr->next_client;
            printf("%d, ", curr->client_number);
        }
        printf("\n")
    }
    if(resigned == NULL){ //wypisywanie kolejki klientów, którzy zrezygnowali
        puts("Resigned queue is empty");
    }
    else{
        printf("Resigned queue: ")
        Queue *curr = resigned;
        printf("%d, ", curr->client_number);
        while(curr->next_client != NULL){
            curr = curr->next_client;
            printf("%d, ", curr->client_number);
        }
        printf("\n")
    }
}

void add_to_waiting_queue(int number){ //dodawanie do kolejki klientów oczekujących
    Queue *new = (Queue*)malloc(sizeof(Queue));
    new->client_number = number;
    new->next_client = NULL;
    if( /* liczba oczekujących == 0*/){ //pusta kolejka
        waiting = new; //ustawienie pierwszego klienta
    }
    else{
        last_waiting->next_client = new; //dodanie klienta do kolejki
    }
    last_waiting = new; //ustawienie ostatniego klienta
    //liczba oczekujących +1
}

void add_to_resigned_queue(int number){ //dodawanie do kolejki klientów, którzy zrezygnowali
    Queue *new = (Queue*)malloc(sizeof(Queue));
    new->client_number = number;
    new->next_client = NULL;
    if( last_waiting == NULL){ //pusta kolejka
        resigned = new; //ustawianie pierwszego elementu
    }
    else{
        last_waiting->next_client = new; //dodawanie klienta do kolejki
    }
    last_waiting = new; //ustawienie ostatniego elementu kolejki
}

void delete_from_waiting_queue(){ //usuwanie pierwszego klienta z kolejki oczekujących
    if(waiting->next_client == NULL){ //jeśli był sam to zwalniamy pamięć
        free(waiting);
    }
    else{ //jeśli nie to usuwamy pierwszego klienta z kolejki
        Queue* first = waiting;
        waiting = first->next_client;
        free(first);
    }
}


void *printString(void *ptr);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    //sprawdzanie jakie opcje podał użytkownik (znowu getopt jak poprzednio tylko parametry inne)


    // drukarka z mutexem
    pthread_t thread1, thread2;
    int iret1;

    iret1 = pthread_create( &thread1, NULL, printString, "HELLO WORLD ");
    if(iret1) {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

    iret1 = pthread_create( &thread2, NULL, printString, "ala ma kota ");
    if(iret1) {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);

    exit(0);
}

// "drukarka" drukujaca na ekran po jednym znaku
void screenPrinter(char c) {
    printf("%c\n",c);
    // drukarka drukuje 4 znaki/s
    usleep(250*1000);
}


void *printString( void *ptr ) {
    char *message;
    message = (char *) ptr;
    int len = strlen(message);
    int i = 0;

    // drukowanie wiadomosci znak po znaku
    for(i=0; i<len; i++){
        screenPrinter(message[i]);
}