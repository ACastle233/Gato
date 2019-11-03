#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
//#define DEBUG
#define EXAMPLE_GROUP "239.0.0.1"

int player_count = 0;
int port_global;
pthread_mutex_t mutexcount;

void error(const char *msg)
{
    perror(msg);
    pthread_exit(NULL);
}

/*
 * Funciones de lectura de socket
 */

/* Lee int de socket cliente. */
int recv_int(int cli_sockfd)
{
    int msg = 0;
    int n = read(cli_sockfd, &msg, sizeof(int));
    
    if (n < 0 || n != sizeof(int)) /* Cliente desconectado. */
        return -1;

    #ifdef DEBUG
    printf("[DEBUG] Entero recibido: %d\n", msg);
    #endif 
    
    return msg;
}

/*
 * Funciones de escritura de socket
 */

/* Escribe mensaje a un socket cliente. */
void write_client_msg(int cli_sockfd, char * msg)
{
    int n = write(cli_sockfd, msg, strlen(msg));
    if (n < 0)
        error("ERROR escribiendo mensaje a socket cliente");
}

/* Escribe un int a un socket cliente. */
void write_client_int(int cli_sockfd, int msg)
{
    int n = write(cli_sockfd, &msg, sizeof(int));
    if (n < 0)
        error("ERROR escribiendo entero a socket cliente");
}

/* Escribe un mensaje a ambos clientes. */
void write_clients_msg(int * cli_sockfd, char * msg)
{
    write_client_msg(cli_sockfd[0], msg);
    write_client_msg(cli_sockfd[1], msg);
}

/* Escribe un int a ambos clientes. */
void write_clients_int(int * cli_sockfd, int msg)
{
    write_client_int(cli_sockfd[0], msg);
    write_client_int(cli_sockfd[1], msg);
}

/*
 * Funciones de conexion
 */

/* Configura listener socket. */
int setup_listener(int portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    /* Get a socket to listen on */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR al abrir socket cliente.");
    
    /* Zero out the memory for the server information */
    memset(&serv_addr, 0, sizeof(serv_addr));
    
	/* set up the server info */
    serv_addr.sin_family = AF_INET;	
    serv_addr.sin_addr.s_addr = INADDR_ANY;	
    serv_addr.sin_port = htons(portno);		

    /* Bind the server info to the listener socket. */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR binding listener socket.");

    #ifdef DEBUG
    printf("[DEBUG] Listener set.\n");    
    #endif 

    /* Return the socket number. */
    return sockfd;
}

/* Configuracion de los sockets cliente y su conexion. */
void get_clients(int lis_sockfd, int * cli_sockfd)
{
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    
    #ifdef DEBUG
    printf("[DEBUG] Escucha por clientes...\n");
    #endif 

    /* Escucha por dos clientes. */
    int num_conn = 0;
    while(num_conn < 2)
    {
        /* Escucha por clientes. */
	    listen(lis_sockfd, 253 - player_count);
        
        /* Zero out memory for the client information. */
        memset(&cli_addr, 0, sizeof(cli_addr));

        clilen = sizeof(cli_addr);
	
	    /*Acepta la conexion desde el cliente. */
        cli_sockfd[num_conn] = accept(lis_sockfd, (struct sockaddr *) &cli_addr, &clilen);
    
        if (cli_sockfd[num_conn] < 0)
            /* Algo no ha salido bien. */
            error("ERROR al aceptar conexion del cliente.");

        #ifdef DEBUG
        printf("[DEBUG] Conexion aceptada del cliente %d\n", num_conn);
        #endif 
        
        /* Send the client it's ID. */
        write(cli_sockfd[num_conn], &num_conn, sizeof(int));
        
        #ifdef DEBUG
        printf("[DEBUG]Envia clinte %d su ID.\n", num_conn); 
        #endif 
        
        /* Incrementa el contador del jugador. */
        pthread_mutex_lock(&mutexcount);
        player_count++;
        printf("El numero de jugadores es ahora %d.\n", player_count);
        pthread_mutex_unlock(&mutexcount);

        if (num_conn == 0) {
            /* Envia "HLD" al primer cliente para que sepa el usuario que el servidor esta esperando un segundo jugador. */
            write_client_msg(cli_sockfd[0],"HLD");
            
            #ifdef DEBUG
            printf("[DEBUG] Told client 0 to hold.\n");
            #endif 
        }

        num_conn++;
    }
}

/*
 * Game Functions
 */

/* Obtiene movimiento del cliente */
int get_player_move(int cli_sockfd)
{
    #ifdef DEBUG
    printf("[DEBUG] Obteniendo movimiento del jugador...\n");
    #endif
    
    /* Decirle al jugador que haga un movimiento. */
    write_client_msg(cli_sockfd, "TRN");

    /* Recibe el movimiento de los jugadores. */
    return recv_int(cli_sockfd);
}

/* Checa los movimientos de los jugadores. */
int check_move(char board[][3], int move, int player_id)
{
    if ((move == 9) || (board[move/3][move%3] == ' ')) { /* Movimiento valido. */
        
        #ifdef DEBUG
        printf("[DEBUG] Movimiento valido del Jugador %d'.\n", player_id);
        #endif
        
        return 1;
   }
   else { /* Movimiento invalido. */
       #ifdef DEBUG
       printf("[DEBUG] Movimiento invalido del Jugador %d.\n", player_id);
       #endif
    
       return 0;
   }
}

/* Updates the board with a new move. */
void update_board(char board[][3], int move, int player_id)
{
    board[move/3][move%3] = player_id ? 'X' : 'O';
    
    #ifdef DEBUG
    printf("[DEBUG] Tablero actualizado.\n");
    #endif
}

/* Dibuja la tabla. */
void draw_board(char board[][3])
{
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

/* Envia el tablero actualizado a ambos clientes. */
void send_update(int * cli_sockfd, int move, int player_id)
{
    #ifdef DEBUG
    printf("[DEBUG] Envia actualizacion...\n");
    #endif
    
    /* Señal de actualizacion */    
    write_clients_msg(cli_sockfd, "UPD");

    /* Envia el id del jugador que hizo el movimiento. */
    write_clients_int(cli_sockfd, player_id);
    
    /* Envia el movimiento */
    write_clients_int(cli_sockfd, move);
    
    #ifdef DEBUG
    printf("[DEBUG] Actualizacion enviada.\n");
    #endif
}

/* Envio del numero de jugador al cliente. */
void send_player_count(int cli_sockfd)
{
    write_client_msg(cli_sockfd, "CNT");
    write_client_int(cli_sockfd, player_count);

    #ifdef DEBUG
    printf("[DEBUG] Envio del contador al jugador.\n");
    #endif
}

/* Checa el tablero si hay ganador. */
int check_board(char board[][3], int last_move)
{
    #ifdef DEBUG
    printf("[DEBUG] Checa por un ganador...\n");
    #endif
   
    int row = last_move/3;
    int col = last_move%3;

    if ( board[row][0] == board[row][1] && board[row][1] == board[row][2] ) { /* Checa fila. */
        #ifdef DEBUG
        printf("[DEBUG] Gana por fila %d.\n", row);
        #endif 
        return 1;
    }
    else if ( board[0][col] == board[1][col] && board[1][col] == board[2][col] ) { /* Checa columna. */
        #ifdef DEBUG
        printf("[DEBUG] Gano por columna %d.\n", col);
        #endif 
        return 1;
    }
    else if (!(last_move % 2)) { /* Checa las diagonales */
        if ( (last_move == 0 || last_move == 4 || last_move == 8) && (board[1][1] == board[0][0] && board[1][1] == board[2][2]) ) {  /* Checa diagonal invertida. */
            #ifdef DEBUG
            printf("[DEBUG] Gana por diagonal invertida.\n");
            #endif 
            return 1;
        }
        if ( (last_move == 2 || last_move == 4 || last_move == 6) && (board[1][1] == board[0][2] && board[1][1] == board[2][0]) ) { /* Checa diagonal. */
            #ifdef DEBUG
            printf("[DEBUG] Gana por diagonal.\n");
            #endif 
            return 1;
        }
    }

    #ifdef DEBUG
    printf("[DEBUG] No ganador, aun.\n");
    #endif
    
    /* Nadie gana aun. */
    return 0;
}

/*Corre el juego entre dos clientes. */
void *run_game(void *thread_data) 
{
    int *cli_sockfd = (int*)thread_data; /* Sockets cliente. */
    char board[3][3] = { {' ', ' ', ' '}, /* Tablero */ 
                         {' ', ' ', ' '}, 
                         {' ', ' ', ' '} };

    printf("Game on!\n");
    
    /* Mensaje de entrada. */
    write_clients_msg(cli_sockfd, "SRT");

    #ifdef DEBUG
    printf("[DEBUG] Envio de mensaje que ha comenzado.\n");
    #endif

    draw_board(board);
    
    int prev_player_turn = 1;
    int player_turn = 0;
    int game_over = 0;
    int turn_count = 0;
    while(!game_over) {
        /* Dile al otro jugador que espere */
        if (prev_player_turn != player_turn)
            write_client_msg(cli_sockfd[(player_turn + 1) % 2], "WAT");

        int valid = 0;
        int move = 0;
        while(!valid) { /*Checar hasta que el movimiento del jugador sea valido. */
            move = get_player_move(cli_sockfd[player_turn]);
            if (move == -1) break; /* Error leer cliente socket. */

            printf("Jugador %d jugo la posicion %d\n", player_turn, move);
                
            valid = check_move(board, move, player_turn);
            if (!valid) { /* Movimiento invalido. */
                printf("Movimiento invalido. Intentelo otra vez...\n");
                write_client_msg(cli_sockfd[player_turn], "INV");
            }
        }

	    if (move == -1) { /* Error reading from client. */
            printf("Jugador desconectado.\n");
            break;
        }
        else if (move == 9) { /* Envia al cliente el numero de jugadores. */
            prev_player_turn = player_turn;
            send_player_count(cli_sockfd[player_turn]);
        }
        else {
            /* Actualiza el tablero y notifica. */
            update_board(board, move, player_turn);
            send_update( cli_sockfd, move, player_turn );
                
            /* Redibuja tablero. */
            draw_board(board);

            /* Checa si hay ganador/perdedor. */
            game_over = check_board(board, move);
            
            if (game_over == 1) { /* Ganador. */
                write_client_msg(cli_sockfd[player_turn], "WIN");
                write_client_msg(cli_sockfd[(player_turn + 1) % 2], "LSE");
                printf("Jugador %d gana, FINISIH HIM!!.\n", player_turn);
            }
            else if (turn_count == 8) { /* Empate. */
                printf("Pinta.\n");
                write_clients_msg(cli_sockfd, "DRW");
                game_over = 1;
            }

            /* Siguiente jugador. */
            prev_player_turn = player_turn;
            player_turn = (player_turn + 1) % 2;
            turn_count++;
        }
    }

    printf("Game over.\n");

	/* Cierra los sockets cliente y decremente el contador de no. de jugadores. */
    close(cli_sockfd[0]);
    close(cli_sockfd[1]);

    pthread_mutex_lock(&mutexcount);
    player_count--;
    printf("Numero de jugadores es %d.", player_count);
    player_count--;
    printf("Numero de jugadores es %d.", player_count);
    pthread_mutex_unlock(&mutexcount);
    
    free(cli_sockfd);

    pthread_exit(NULL);
}
//Hilo del envio de datagramas con el numero de puerto al que se esta conectando
void*envioDTGM()
{
    struct sockaddr_in addr;
   int addrlen, sock, cnt;
   char message[50];

   /* set up socket */
   sock = socket(AF_INET, SOCK_DGRAM, 0);
   if (sock < 0) {
     perror("socket");
     exit(1);
   }
   bzero((char *)&addr, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port = htons(9000);
   addrlen = sizeof(addr);

    /* send */
    addr.sin_addr.s_addr = inet_addr(EXAMPLE_GROUP);
    while (1) {
        time_t t = time(0);
        sprintf(message, "%d", port_global);
        printf("Enviando: %s\n", message);
        cnt = sendto(sock, message, sizeof(message), 0,(struct sockaddr *) &addr, addrlen);
        if (cnt < 0) {
            perror("sendto");
            exit(1);
        }
        sleep(5);
    }
   
}
//Hilo del juego, inicia la partida con dos jugadores
void*iniciaJuego()
{
    int lis_sockfd = setup_listener(port_global); /* Listener socket. */
    pthread_mutex_init(&mutexcount, NULL);
    printf("Inicia Juego");
    while (1) {
        if (player_count <= 2) { /* Lanza el juego si hay espacio. */  
            int *cli_sockfd = (int*)malloc(2*sizeof(int)); /* Cliente sockets */
            memset(cli_sockfd, 0, 2*sizeof(int));
            
            /* Get two clients connected. */
            get_clients(lis_sockfd, cli_sockfd);
            
            #ifdef DEBUG
            printf("[DEBUG] Starting new game thread...\n");
            #endif

            pthread_t thread; 
            int result = pthread_create(&thread, NULL, run_game, (void *)cli_sockfd); /* Empieza un nuevo hilo para el juego. */
            if (result){
                printf("Fallo en la creacion de hilo con codigo %d\n", result);
                exit(-1);
            }
            
            #ifdef DEBUG
            printf("[DEBUG] New game thread started.\n");
            #endif
        }
    }

    close(lis_sockfd);

    pthread_mutex_destroy(&mutexcount);
    pthread_exit(NULL); 
}
/* 
 * Main 
 */

int main(int argc, char *argv[])
{   
    /* Especificacion de puerto. */
    if (argc < 2) {
        fprintf(stderr,"ERROR, falta puerto\n");
        exit(1);
    }
    port_global=atoi(argv[1]);
    pthread_t hiloMulti,hiloJuego;
    
    pthread_create(&hiloMulti,NULL,envioDTGM,NULL);//Hilo del envio del puerto por Multicast
    pthread_create(&hiloJuego,NULL,iniciaJuego,NULL);//Hilo de la partida de gato
    pthread_join(hiloMulti,NULL);
    pthread_join(hiloJuego,NULL);
    

}
