#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

void error(const char *msg)
{
    #ifdef DEBUG
    perror(msg);
    #else
    printf("Servidor apagado o jugador desconectado.\nGame over.\n");
    #endif 

    exit(0);
}

/*
 * Funciones de lectura de socket
 */

/* Lee mensaje desde el servidor. */
void recv_msg(int sockfd, char * msg)
{
    /* Mensajes de 3 bytes. */
    memset(msg, 0, 4);
    int n = read(sockfd, msg, 3);
    
    if (n < 0 || n != 3) /* Servidor u otro cliente desconectado. */ 
        error("ERROR al leer mensaje de servidor.");

    #ifdef DEBUG
    printf("[DEBUG] Mensaje recibido: %s\n", msg);
    #endif 
}

/* Lee un int del servidor. */
int recv_int(int sockfd)
{
    int msg = 0;
    int n = read(sockfd, &msg, sizeof(int));
    
    if (n < 0 || n != sizeof(int)) 
        error("ERROR reading int from server socket");
    
    #ifdef DEBUG
    printf("[DEBUG] Recibe el int: %d\n", msg);
    #endif 
    
    return msg;
}

/*
 * Funciones de escritura
 */

/* Escribe un int al servidor. */
void write_server_int(int sockfd, int msg)
{
    int n = write(sockfd, &msg, sizeof(int));
    if (n < 0)
        error("ERROR writing int to server socket");
    
    #ifdef DEBUG
    printf("[DEBUG] Wrote int to server: %d\n", msg);
    #endif 
}

/*
 * Funciones de conexion
 */

/* Configura la conexion al servidor. */
int connect_to_server(char * hostname, int portno)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
 
    /* Obtiene un socket. */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
    if (sockfd < 0) 
        error("ERROR  al abrir el socket al servidor.");
	
    /* Obtienes direccion. */
    server = gethostbyname(hostname);
	
    if (server == NULL) {
        fprintf(stderr,"ERROR, no hay host\n");
        exit(0);
    }
	
	/* Zero out memory for server info. */
	memset(&serv_addr, 0, sizeof(serv_addr));

	/* Informacion del servidor. */
    serv_addr.sin_family = AF_INET;
    memmove(server->h_addr_list, &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno); 

	/* Haz la conexion */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR conectando a servidor");

    #ifdef DEBUG
    printf("[DEBUG] Conectado al server.\n");
    #endif 
    
    return sockfd;
}

/*
 * Game Functions
 */

/* Dibuja el tablero. */
void draw_board(char board[][3])
{
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

/* Obtiene el turno del jugador y lo envia al server. */
void take_turn(int sockfd)
{
    char buffer[10];
    
    while (1) { /* Pregunta hasta que reciba. */ 
        printf("Teclea 0-8 para hacer un movimiento, o 9 para ver el numero de jugadores: ");
	    fgets(buffer, 10, stdin);
	    int move = buffer[0] - '0';
        if (move <= 9 && move >= 0){
            printf("\n");
            /* Envia movimiento del jugador al server. */
            write_server_int(sockfd, move);   
            break;
        } 
        else
            printf("\nInvalido, intentalo de nuevo.\n");
    }
}

/* Obtiene actualizacion del servidor. */
void get_update(int sockfd, char board[][3])
{
    /* Obtiene la actualizacion. */
    int player_id = recv_int(sockfd);
    int move = recv_int(sockfd);

    /* Actualiza. */
    board[move/3][move%3] = player_id ? 'X' : 'O';    
}

/*
 * Main 
 */

int main(int argc, char *argv[])
{
    /* Especificacion de puerto. */
    if (argc < 2) {
       fprintf(stderr,"uso %s port\n", argv[0]);
       exit(0);
    }

    /* Conecta con el servidor. */
    int sockfd = connect_to_server("127.0.0.1", atoi(argv[1]));

    /* Recibe cliente ID. */
    int id = recv_int(sockfd);

    #ifdef DEBUG
    printf("[DEBUG] Cliente ID: %d\n", id);
    #endif 

    char msg[4];
    char board[3][3] = { {' ', ' ', ' '}, /* Tablero*/
                         {' ', ' ', ' '}, 
                         {' ', ' ', ' '} };

    printf("GATO\n------------\n");

    /* Espera a que el juego comienza. */
    do {
        recv_msg(sockfd, msg);
        if (!strcmp(msg, "HLD"))
            printf("Esperando al segundo jugador...\n");
    } while ( strcmp(msg, "SRT") );

    /* Ha comenzado el juego... */
    printf("Game on!\n");
    printf("Tu eres %c's\n", id ? 'X' : 'O');

    draw_board(board);

    while(1) {
        recv_msg(sockfd, msg);

        if (!strcmp(msg, "TRN")) { /* Toma un turno. */
	        printf("Tu movimiento...\n");
	        take_turn(sockfd);
        }
        else if (!strcmp(msg, "INV")) { /* Movimiento invalido. Note that a "TRN" message will always follow an "INV" message, so we will end up at the above case in the next iteration. */
            printf("Ya se jugo esa posicion. Vuelve a intentar.\n"); 
        }
        else if (!strcmp(msg, "CNT")) { /* Server is sending the number of active players. Note that a "TRN" message will always follow a "CNT" message. */
            int num_players = recv_int(sockfd);
            printf("Ahorita hay %d juagdores activos.\n", num_players); 
        }
        else if (!strcmp(msg, "UPD")) { /* Servidor esta enviando el tablero actualizado. */
            get_update(sockfd, board);
            draw_board(board);
        }
        else if (!strcmp(msg, "WAT")) { /* Espera por otro jugador a que tome un turno. */
            printf("Esperando movimiento del otro jugador...\n");
        }
        else if (!strcmp(msg, "WIN")) { /* Ganador. */
            printf("Ganaste!\n");
            break;
        }
        else if (!strcmp(msg, "LSE")) { /* Perdedor. */
            printf("Perdiste.\n");
            break;
        }
        else if (!strcmp(msg, "DRW")) { /* Empate. */
            printf("Empate.\n");
            break;
        }
        else /* Algo salio mal... */
            error("Unknown message.");
    }
    
    printf("Game over.\n");

    /* Cierra servidor y cierra. */
    close(sockfd);
    return 0;
}
