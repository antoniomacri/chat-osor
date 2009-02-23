/////////////////////////////////////////////////////////////////////
//
// File: chat_server.c
//
// Nome: Antonio
// Cognome: Macri'
// Matricola: 415591
//


/////////////////////////////////////////////////////////////////////
//
// Include's
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include "commands.h"


/////////////////////////////////////////////////////////////////////
//
// Define's
//

#define BACKLOG     10
#define NUM_THREAD  2


/////////////////////////////////////////////////////////////////////
//
// Utilita'
//

// ascii to unsigned int
int atoui(const char s[])
{
    int i, n;
    for (i=0, n=0; s[i] >= '0' && s[i] <= '9'; i++)
        n = 10 * n + (s[i] - '0');
    return (s[i] == '\0') ? n : -1;
}

inline int min(int a, int b) { return a<b ? a : b; }


/////////////////////////////////////////////////////////////////////
//
// Macro
//

#define SEND(sd,ptr,len) send(sd, ptr, len, 0) < len
#define RECV(sd,ptr,len) recv(sd, ptr, len, 0) < len


/////////////////////////////////////////////////////////////////////
//
// Definizioni di tipo e dichiarazioni di variabili globali
//

struct {
    pthread_t  thread_id;
          int  client_sd;
         char  username[MAX_USERNAME_LEN+1];
        sem_t  mutex;
        short  working;
        short  accepts_msg;
}  data[NUM_THREAD];

sem_t  free_threads;
pthread_mutex_t mutex_usernames;
    

/////////////////////////////////////////////////////////////////////
//
// Main
//

int create_connection(const char *str_host, const char *str_port)
{
    struct sockaddr_in sa;
    int port, sd = socket(PF_INET, SOCK_STREAM, 0);
	if (sd < 0)  {
	    perror("Errore nella creazione del socket");
	    return -1;
	}
	
	port = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &port, sizeof(int));

    memset(&sa, 0x00, sizeof(sa));

	if (!inet_aton(str_host, &sa.sin_addr))  {
	    printf("Errore: la stringa (%s) non specifica un indirizzo "
	           "IP valido!\n", str_host);
	    return -1;
	}

	port = atoui(str_port);
	if (port < 0)  {
	    printf("Errore: la stringa (%s) non specifica una porta "
	           "valida!\n", str_port);
	    return -1;
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short)port);

    if (bind(sd, (struct sockaddr*) &sa, sizeof(sa)) < 0)  {
        perror("Errore nella bind");
        return -1;
    }
	printf("MAIN: Indirizzo: %s (Porta: %hu)\n",
	       inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));

    if (listen(sd, BACKLOG) < 0)  {
        perror("Errore nella listen");
        return -1;
    }

    return sd;
}


void do_who(const int i)
{
    int j, tot_len = 0;
    char response[MAX_MSG_LEN+1];

    pthread_mutex_lock(&mutex_usernames);
    for (j = 0; j < NUM_THREAD && tot_len < MAX_MSG_LEN; j++)
    {
        if (data[j].username[0])  {
            int len = min(strlen(data[j].username),
                          MAX_MSG_LEN-tot_len);
            strncpy(response + tot_len, data[j].username, len);
            tot_len += len;
            if (tot_len < MAX_MSG_LEN)  {
                strcpy(response + tot_len, " ");
                tot_len++;
            }
        }
    }
    pthread_mutex_unlock(&mutex_usernames);

    if (SEND(data[i].client_sd, &tot_len, sizeof(int)) ||
        SEND(data[i].client_sd, response, tot_len))
    {
        printf("THREAD %d: ", i);
        perror("Errore nell'invio della risposta al client");
    }
    else  {
        printf("THREAD %d: Invio al client \"%s\" della risposta "
               "\"%s\"\n", i, data[i].username, response);
    }
}


void do_send_msg(const int i)
{
    int len, j, cmd;
    char destination[MAX_USERNAME_LEN+1];
    char msg[MAX_MSG_LEN+1];

    if (RECV(data[i].client_sd, &len, sizeof(int)))  {
        printf("THREAD %d: ", i);
        perror("Errore nella ricezione della lunghezza del "
               "messaggio");
        return;
    }

    if (RECV(data[i].client_sd, msg, len))  {
        printf("THREAD %d: ", i);
        perror("Errore nella ricezione del messaggio");
        return;
    }
    msg[len] = '\0';
    
    printf("THREAD %d: Ricevuto da \"%s\" il messaggio \"%s\"\n",
             i, data[i].username, msg);

    if (RECV(data[i].client_sd, &len, sizeof(int)))  {
        printf("THREAD %d: ", i);
        perror("Errore nella ricezione della lunghezza del "
               "destinatario");
        return;
    }
    if (RECV(data[i].client_sd, destination, len))  {
        printf("THREAD %d: ", i);
        perror("Errore nella ricezione del destinatario");
        return;
    }
    destination[len] = '\0';

    printf("THREAD %d: Ricevuto da \"%s\" il destinatario \"%s\"\n",
             i, data[i].username, destination);


    cmd = CMD_CLIENT_INEXISTENT;
    pthread_mutex_lock(&mutex_usernames);
    for (j = 0; j < NUM_THREAD; j++)  {
        if (!strcmp(destination, data[j].username))  {
            cmd = data[j].accepts_msg ? CMD_OK : CMD_CLIENT_NON_RECV;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_usernames);

    if (cmd == CMD_OK)  {
        len = strlen(msg);
        if (SEND(data[j].client_sd, &len, sizeof(int)) ||
            SEND(data[j].client_sd, msg, len))  {
            printf("THREAD %d: ", i);
            perror("Errore nell'invio del messaggio al "
                   "destinatario");
            cmd = CMD_ERROR;
        }
        else  {
            len = strlen(data[i].username);
            if (SEND(data[j].client_sd, &len, sizeof(len)) ||
                 SEND(data[j].client_sd, data[i].username, len))  {
                printf("THREAD %d: ", i);
                perror("Errore nell'invio al destinatario del nome "
                       "utente del mittente");
                cmd = CMD_ERROR;
            }
            else  {
                printf("THREAD %d: Inviato da \"%s\" a \"%s\" il "
                       "messaggio \"%s\"\n",
                       i, data[i].username, destination, msg);
            }
        }
        data[j].accepts_msg = 0;
    }

    if (SEND(data[i].client_sd, &cmd, sizeof(int)))  {
        printf("THREAD %d: ", i);
        perror("Errore nell'invio della notifica al mittente");
    }
}

void do_recv_msg(const int i)
{
    data[i].accepts_msg = 1;
}

int do_login(int i)
{
    while(1)  {
        int len;
        char username[MAX_USERNAME_LEN+1];

        if (RECV(data[i].client_sd, &len, sizeof(int)))  {
            printf("THREAD %d: ", i);
            perror("Errore nella ricezione della lunghezza del nome "
                   "utente");
            return -1;
        }
        else if (len > MAX_USERNAME_LEN)  {
            printf("THREAD %d: ", i);
            perror("Errore: la lunghezza del nome utente e' "
                   "maggiore della massima consentita.");
            return -1;
        }
        else if (recv(data[i].client_sd, username, len, 0) < len)  {
            printf("THREAD %d: ", i);
            perror("Errore nella ricezione del nome utente");
            return -1;
        }
        else  {
            int cmd = CMD_OK, j;

            username[len] = '\0';

            pthread_mutex_lock(&mutex_usernames);
            for (j = 0; j < NUM_THREAD; j++)  {
                if (i != j && !strcmp(username, data[j].username))  {
                    cmd = CMD_ERROR;
                    break;
                }
            }
            if (SEND(data[i].client_sd, &cmd, sizeof(cmd)))  {
                printf("THREAD %d: ", i);
                perror("Impossibile inviare dati al client");
                pthread_mutex_unlock(&mutex_usernames);
                return -1;
            }
            else if (cmd == CMD_OK)  {
                strncpy(data[i].username, username, len);
                pthread_mutex_unlock(&mutex_usernames);
                return 0;
            }
            printf("THREAD %d: Il client ha richiesto un nome "
                   "utente già utilizzato\n", i);
            pthread_mutex_unlock(&mutex_usernames);
        }
    }
}


void * process_client(void * arg)
{
    int i = (int)arg;

    while (1)
    {
        int cmd;

        printf("THREAD %d: pronto.\n", i);

        while (sem_wait(&data[i].mutex))  { }
        // data[i].working = 1;  // messo in realta' ad 1 nel main

        cmd = CMD_OK;
        if (SEND(data[i].client_sd, &cmd, sizeof(int)))  {
            perror("Impossibile inviare dati al client");
            close (data[i].client_sd);
            data[i].working = 0;
            sem_post(&free_threads);
            continue;
        }

        if (do_login(i))  {
            close (data[i].client_sd);
            data[i].working = 0;
            sem_post(&free_threads);
            continue;
        }

        printf("THREAD %d: %s si e' connesso\n", i,data[i].username);

        do {
            int ret = recv(data[i].client_sd, &cmd, sizeof(cmd), 0);

            if (ret == 0)  {
                // Se la recv ritorna 0, non imposta errno, percio':
                // non chiamare la perror() qui!
                printf("THREAD %d: Il client ha chiuso la "
                       "connessione\n", i);
                break;
            }
            else if (ret == sizeof(cmd))  {
                switch(cmd)
                {
                    case CMD_WHO:
                        printf("THREAD %d: Ricevuto comando "
                               "\"who\"\n", i);
                        do_who(i);
                    break;
                    case CMD_SEND_MSG:
                        printf("THREAD %d: Ricevuto comando "
                               "\"send_msg\"\n", i);
                        do_send_msg(i);
                    break;
                    case CMD_RECV_MSG:
                        printf("THREAD %d: Ricevuto comando "
                               "\"recv_msg\"\n", i);
                        do_recv_msg(i);
                    break;
                    case CMD_BYE:
                        printf("THREAD %d: Ricevuto comando "
                               "\"bye\"\n", i);
                        continue;
                    break;
                    default:
                        printf("THREAD %d: Comando non riconosciuto "
                               "(codice: %d)\n", i, cmd);
                }
            }
            else  {
                printf("THREAD %d: ", i);
                perror("Errore nella ricezione della richiesta dal "
                       "client");
            }
        } while (cmd != CMD_BYE);

        printf("THREAD %d: %s si e' disconnesso\n",
               i, data[i].username);

        close (data[i].client_sd);

        pthread_mutex_lock(&mutex_usernames);          // non
        data[i].username[0] = '\0';      // strettissimamente
        pthread_mutex_unlock(&mutex_usernames); // necessario

        data[i].working = 0;
        sem_post(&free_threads);
    }
    return NULL;
}


void create_thread_pool()
{
    int i;
    for(i = 0; i < NUM_THREAD; i++)  {
        data[i].client_sd = -1;
        data[i].username[0] = '\0';
        data[i].working = 0;
        sem_init(&data[i].mutex, 0, 0);
        if (pthread_create(&data[i].thread_id, NULL,
                           process_client, (void*)i))
        {
            perror("Errore nella creazione del pool di thread");
            exit(-1);
        }
    }
}


void destroy_thread_pool()
{
    int i;
    for(i = 0; i < NUM_THREAD; i++)  {
        sem_destroy(&data[i].mutex);
        if (pthread_kill(data[i].thread_id, SIGKILL))  {
            printf("Errore nella distruzione del thread %d", i);
            perror("");
        }
    }
}


static int sd;

void onkill(int p)
{
    printf("\rRicevuto segnale di terminazione.\n");
    
    destroy_thread_pool();

    sem_destroy(&free_threads);
    pthread_mutex_destroy(&mutex_usernames);
    close(sd);
    exit(0);
}


int main(int argc, char *argv[])
{
	if (argc != 3)  {
	    printf("Usage:  %s <host> <port>\n", *argv);
	    return -1;
	}

    pthread_mutex_init(&mutex_usernames, NULL);
    sem_init(&free_threads, 0, NUM_THREAD);

    create_thread_pool();

	sd = create_connection(argv[1], argv[2]);
    if (sd < 0)
        return -1;

    signal(SIGINT, onkill);

    do {
        struct sockaddr_in sa_client;
        int i;
        socklen_t sa_len = sizeof(sa_client);

        int sd_client;
        sd_client= accept(sd, (struct sockaddr*)&sa_client, &sa_len);
        if (sd_client < 0)  {
            perror("MAIN: Errore nella accept");
            continue;
        }

        printf("MAIN: Connessione accettata dal client %s "
               "(porta %hu)\n", inet_ntoa(sa_client.sin_addr),
               ntohs(sa_client.sin_port));

        while (sem_wait(&free_threads))  {  }

        for (i = 0; i < NUM_THREAD; i++)  {
            if (data[i].working == 0)  {
                // Il working lo devo mettere ad 1 qui nel main e non
                // dal thread
                data[i].working = 1;
                data[i].client_sd = sd_client;
                printf("MAIN: E' stato selezionato il thread %d\n",
                       i);
                sem_post(&data[i].mutex);
                break;
            }
        }

    }  while(1);

    return 0;
}

