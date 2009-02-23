/////////////////////////////////////////////////////////////////////
//
// File: chat_client.c
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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "commands.h"


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


/////////////////////////////////////////////////////////////////////
//
// Define's & Macro
//

#define SEND(sd,ptr,len) send(sd, ptr, len, 0) < len
#define RECV(sd,ptr,len) recv(sd, ptr, len, 0) < len


/////////////////////////////////////////////////////////////////////
//
// Comandi
//

void do_help()
{
    puts("\nSono disponibili i seguenti comandi:");
    puts(" * help --> mostra l'elenco dei comandi disponibili");
    puts(" * who --> mostra l'elenco dei client connessi al server");
    puts(" * send_msg --> invia un messaggio a un client specifico");
    puts(" * recv_msg --> pone il client in attesa di un messaggio");
    puts(" * bye --> disconnette il client dal server\n");
}

void do_who(int sd)
{
    int len, cmd = CMD_WHO;
    char *buffer;

    if (SEND(sd, &cmd, sizeof(int)))  {
        perror("Errore nell'invio del comando al server");
        return;
    }
    if (RECV(sd, &len, sizeof(int)))  {
        perror("Errore nella ricezione della risposta dal server");
        return;
    }

    buffer = malloc(len+1);
    if (buffer == NULL)  {
        perror("Errore di memoria");
        return;
    }
    else if (RECV(sd, buffer, len))  {
        perror("Errore nella ricezione della risposta dal server");
    }
    else  {
        buffer[len] = '\0';
        printf("Client connessi al server: %s\n\n", buffer);
    }
    free(buffer);
}

void do_send_msg(int sd)
{
    int cmd = CMD_SEND_MSG, len;
    char dest[MAX_USERNAME_LEN+1];
    char msg[MAX_MSG_LEN+1];

    printf("Inserire messaggio: ");
    fgets(msg, sizeof(msg), stdin);
    do  {
        printf("Inserire destinatario: ");
    }  while (!fgets(dest, sizeof(dest), stdin) || *dest == '\n');

    if (SEND(sd, &cmd, sizeof(int)))  {
        perror("Errore nell'invio del comando al server");
        return;
    }

    len = strlen(msg);
    msg[--len] = '\0';
    if (SEND(sd, &len, sizeof(int)) || SEND(sd, msg, len))  {
        perror("Errore nell'invio del messaggio al server");
        return;
    }

    len = strlen(dest);
    dest[--len] = '\0';
    if (SEND(sd, &len, sizeof(int)) || SEND(sd, dest, len))  {
        perror("Errore nell'invio del nome del destinatario al "
               "server");
        return;
    }

    if (RECV(sd, &cmd, sizeof(int)))  {
        perror("Errore nella ricezione della risposta dal server");
        return;
    }
    else if (cmd == CMD_CLIENT_INEXISTENT)  {
        printf("Impossibile inviare il messaggio perche' il client "
               "non esiste.\n");
        return;
    }
    else if (cmd == CMD_CLIENT_NON_RECV)  {
        printf("Impossibile inviare il messaggio perche' il client "
               "non e' in ricezione.\n");
        return;
    }
    else if (cmd == CMD_OK)  {
        printf("Messaggio inviato correttamente\n");
    }
    else  {
        printf("Impossibile recapitare il messaggio al client.\n");
    }
}

void do_recv_msg(int sd)
{
    int cmd = CMD_RECV_MSG;
    int len;
    char sender[MAX_USERNAME_LEN+1];
    char msg[MAX_MSG_LEN+1];

    if (SEND(sd, &cmd, sizeof(int)))  {
        perror("Errore nell'invio del comando al server");
        return;
    }

    printf("Rimango in attesa di un messaggio\n");
    if (RECV(sd, &len, sizeof(int)) || RECV(sd, msg, len))  {
        perror("Errore nella ricezione del messaggio dal server");
        return;
    }
    msg[len] = '\0';

    if (RECV(sd, &len, sizeof(int)) || RECV(sd, sender, len))  {
        perror("Errore nella ricezione del mittente dal server");
        return;
    }
    sender[len] = '\0';

    printf("Messaggio ricevuto da \"%s\": %s\n", sender, msg);
}

void do_bye(int sd)
{
    int cmd = CMD_BYE;
    
    if (send(sd, &cmd, sizeof(int), 0) < sizeof(int))  {
        perror("Errore nell'invio del comando al server");
    }
    if (recv(sd, &cmd, 0, 0) < 0)  {
        perror("Errore nella disconnessione dal server");
    }
    else  {
        printf("Client disconnesso correttamente");
    }
}


/////////////////////////////////////////////////////////////////////
//
// Main
//

int create_connection(const char *str_host, const char *str_port)
{
    int cmd;
    struct sockaddr_in sa;
    int port, sd = socket(PF_INET, SOCK_STREAM, 0);
	if (sd < 0)  {
	    perror("Errore nella creazione del socket");
	    exit(-1);
	}

    memset(&sa, 0x00, sizeof(sa));

	if (!inet_aton(str_host, &sa.sin_addr))  {
	    printf("Errore: la stringa (%s) non specifica un indirizzo "
	           "IP valido!\n", str_host);
	    exit(-1);
	}

	port = atoui(str_port);
	if (port < 0)  {
	    printf("Errore: la stringa (%s) non specifica una porta "
	           "valida!\n", str_port);
	    exit(-1);
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short)port);

    puts("\nIn attesa di connessione...");
    if (connect(sd, (struct sockaddr*) &sa, sizeof(sa)) < 0)  {
        perror("Errore durante la connessione");
    }
    else if (recv(sd, &cmd, sizeof(cmd), 0) < sizeof(cmd))  {
        perror("Errore durante la connessione");
    }
    else if (cmd != CMD_OK)  {
        puts("Il server ha negato l'accesso!");
    }
    else  {
    	printf("Connessione al server %s (porta %hu) effettuata con "
    	       "successo.\n",
    	       inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
    	return sd;
    }
    exit(-1);
}

void do_login(int sd)
{
    while(1)  {
        int len;
        char username[MAX_USERNAME_LEN+1];
        do  {
            printf("Inserisci il tuo nome: ");
        }  while (!fgets(username, sizeof(username), stdin) ||
                  *username == '\n');

        len = strlen(username);
        username[--len] = '\0';

        if (SEND(sd, &len, sizeof(len)) || SEND(sd, username, len))
        {
            perror("Errore durante la fase di login");
            exit(-1);
        }
        if (RECV(sd, &len, sizeof(len))) {
            perror("Errore durante la fase di login");
            exit(-1);
        }
        if (len != CMD_OK)  {
            printf("Esiste gia' un utente di nome \"%s\"\n",
                   username);
        }
        else  {
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    int sd;

	if (argc != 3)  {
	    printf("Usage:  %s <host> <port>\n", *argv);
	    return -1;
	}

    sd = create_connection(argv[1], argv[2]);
    do_help();
    do_login(sd);

    do  {
        char command[10];

        do  {
            printf("> ");
        } while (!fgets(command, sizeof(command), stdin) ||
                 *command == '\n');

        command[strlen(command)-1] = '\0';

        if (!strcmp(command, "help"))
        {
            do_help();
        }
        else if(!strcmp(command, "who"))
        {
            do_who(sd);
        }
        else if(!strcmp(command, "send_msg"))
        {
            do_send_msg(sd);
        }
        else if(!strcmp(command, "recv_msg"))
        {
            do_recv_msg(sd);
        }
        else if(!strcmp(command, "bye"))
        {
            do_bye(sd);
            break;
        }
        else
        {
            do_help();
        }
    } while(1);

    close(sd);
	puts("");
	return 0;
}

