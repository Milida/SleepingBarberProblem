#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

void *printString(void *ptr); //KOD Z MUTEXAMI Z ZAJĘĆ

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int main() {
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

}