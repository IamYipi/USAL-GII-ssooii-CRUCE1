// gcc cruce.c libcruce.a -o cruce -m32
#include "cruce.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
//Constantes
#define MAX 127
#define MIN 2
#define MEMORIA 257
#define PROC 1
#define C1 2
#define C2 3
#define P1 4
#define P2 5
#define CRUCE 6
#define NACER 7
//Manejadora
struct sigaction manejadora;
void matar(int signal);
//Encina
union semun {  
    int val;
    struct semid_ds *buf;  
    unsigned short *array; 
};
//Variables
int semaforo,mcompartida,buzon;
int ppid;
char * puntero_mem = NULL;
int max_proc;
//Struct del buzon
typedef struct mensaje{
	long coord;
	char info;
} mensaje;
//FUNCIONES
void muerte();
void ciclo_semaforico();
void coche();
void peaton();
int pos_valida(struct posiciOn);
struct posiciOn mover_coche(struct posiciOn pos, struct posiciOn * anterior);
struct posiciOn mover_peaton(struct posiciOn pos, struct posiciOn * anterior);
//Funciones semaforos
int semaforo_set_valor(int sem,unsigned short i, int valor);
int semaforo_cambiar_valor(int sem,unsigned short i,int valor);
//Wait
int semaforo_bloqueado(int sem,unsigned short i);
//Signal
int semaforo_desbloqueado(int sem,unsigned short i);
//MAIN
int main(int argc, char * argv[]){
	union semun sem;
	int vel,coord;
	int i,j,size,pid;
	char cad[100];	
	mensaje msg;
//COMPROBACIÓN NUMERO ARGUMENTOS
    if(argc != 3){
        sprintf(cad, "Número de argumentos erróneo, tienen que ser 2 argumentos obligatorios\n");
        size = strlen(cad);
        if(write(0, cad, size) == -1)
        	perror("Error");
        exit(2);
    }
	max_proc = atoi(argv[1]);
	vel = atoi(argv[2]);	
//COMPROBACIÓN NUMERO PROCESOS
    if(max_proc > MAX || max_proc < MIN){
        sprintf(cad, "Lo sentimos, ese numero está fuera de los limites de procesos\n");
        size = strlen(cad);
        if(write(0, cad, size) == -1)
        	perror("Error");
        exit(2);
    } 
//COMPROBACIÓN VELOCIDAD
	if(vel < 0){
		sprintf(cad, "Lo sentimos, ese numero está fuera de los limites de la velocidad\n");
        size = strlen(cad);
        if(write(0, cad, size) == -1)
        	perror("Error");
        exit(2);
	}
//Pid Padre	
	ppid = getpid();
//Manejadora
	manejadora.sa_handler = matar;
    	sigfillset(&manejadora.sa_mask);
   	manejadora.sa_flags = 0;   
	sigaction(SIGINT,&manejadora,NULL);
	sigaction(SIGTERM,&manejadora,NULL);	
//CREACION IPCS	
//Creación de array semáforos
	if((semaforo = semget(IPC_PRIVATE, 8, IPC_CREAT | 0600)) == -1){
		perror("Error al crear los semaforos\n");
		exit(1);
	}
//Creación buzón			
	if((buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
		perror("Error al crear el buzon\n"); 	
		exit(1);
	}
//Segmento memoria compartida		
	if((mcompartida = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0600)) == -1){
		perror("Error al crear la memoria compartida\n");
		exit(1);
	}
//Asignacion de puntero		
	if((puntero_mem = shmat(mcompartida,0,0)) == NULL){					
		perror("Error al conectar el puntero\n");
		exit(1);
	}
//Establecemos valor de los semaforos dependiendo de su funcion
	int val = max_proc - 1;
//SEMAFOROS
//Este semaforo tendran un valor como tantos procesos usaremos
	semaforo_set_valor(semaforo,PROC,val);	
	val = 0;
	//C1
	semaforo_set_valor(semaforo,C1,val);
	val = 0;
	//C2
	semaforo_set_valor(semaforo,C2,val);
	val = 1;
	//P1
	semaforo_set_valor(semaforo,P1,val);
	val = 0;
	//P2
	semaforo_set_valor(semaforo,P2,val);
	val = 1;
	//CRUCE
	semaforo_set_valor(semaforo,CRUCE,val);
	val = 1;
	//NACER
	semaforo_set_valor(semaforo,NACER,val);
//Buzon
//Bucle for que rellena el buzon con todo coordenadas
	for(i = 0; i <= 16; i++){
		for(j = 0; j <= 50; j++){
			msg.coord = 1 + i*100 + j;
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1)kill(0, SIGTERM); 			
		}
	} 
//Inicio programa
	CRUCE_inicio(vel, max_proc, semaforo, puntero_mem);
//Inicio proceso hijo ciclo_semaforico
	if((semaforo_bloqueado(semaforo,PROC)) == -1) kill(0, SIGTERM);
	pid = fork();
	switch(pid){
		case -1:
			perror("Fork error");
			kill(0, SIGTERM);
		case 0:
			ciclo_semaforico();
	}
//Bucle infinito creacion de procesos
	while(1){
//Hacemos un wait cada vez que se crea un proceso, 
//Es decir vamos restando 1 al semaforo de creacion de procesos
//Una vez que llega a valor negativo se bloquea hasta que se libera un proceso
		if((semaforo_bloqueado(semaforo,PROC)) == -1) kill(0,SIGTERM);
		coord = CRUCE_nuevo_proceso();
		pid = fork();
		switch(pid){
			case -1:
				perror("Fork error");
				kill(0, SIGTERM);
			case 0:			
				switch (coord){				
					case COCHE:
						coche();
						
					case PEAToN:		
						peaton();

					default:						
						muerte();
				}
			default:
				break;
		}	
	}
	return 0;
}
//FUNCIONES
//Funcion mata hijos
void muerte(){
	if(semaforo_desbloqueado(semaforo,PROC) == 1){
		kill(0,SIGTERM);
	}
	exit(0);
}
//Ciclo semaforico
void ciclo_semaforico(){
	int i;
	CRUCE_pon_semAforo(SEM_C2, ROJO);
	while(1){
		//P1
		if((semaforo_bloqueado(semaforo,P1)) == -1) kill(0, SIGTERM); 
		CRUCE_pon_semAforo(SEM_P1, ROJO);
		CRUCE_pon_semAforo(SEM_C1, VERDE);
		CRUCE_pon_semAforo(SEM_P2, VERDE);
		//C1
		if((semaforo_desbloqueado(semaforo,C1)) == -1) kill(0, SIGTERM); 
		//P2
		if((semaforo_desbloqueado(semaforo,P2)) == -1) kill(0, SIGTERM); 
		for(i = 0; i < 6; i++){
			pausa();
		}
		//C1
		if((semaforo_bloqueado(semaforo,C1)) == -1) kill(0, SIGTERM); 	
		//P2
		if((semaforo_bloqueado(semaforo,P2)) == -1) kill(0, SIGTERM); 
		CRUCE_pon_semAforo(SEM_C1, AMARILLO);
		CRUCE_pon_semAforo(SEM_P2, ROJO);
		pausa();
		pausa();
		//CRUCE
		if((semaforo_bloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM); 
		CRUCE_pon_semAforo(SEM_C1, ROJO);
		if((semaforo_desbloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM);
		CRUCE_pon_semAforo(SEM_C2, VERDE);
		//C2
		if((semaforo_desbloqueado(semaforo,C2)) == -1) kill(0, SIGTERM);
		for(i = 0; i < 9; i++){
			pausa();
		}
		if((semaforo_bloqueado(semaforo,C2)) == -1) kill(0, SIGTERM); 
		CRUCE_pon_semAforo(SEM_C2, AMARILLO);
		pausa();
		pausa();
		//CRUCE
		if((semaforo_bloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM); 
		CRUCE_pon_semAforo(SEM_C2, ROJO);
		if((semaforo_desbloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM);
		CRUCE_pon_semAforo(SEM_P1, VERDE);
		if((semaforo_desbloqueado(semaforo,P1)) == -1) kill(0, SIGTERM);
		for(i = 0; i < 12; i++){
			pausa();
		}
	}
}
//Funcion movimiento empleando buzones
struct posiciOn mover_coche(struct posiciOn pos, struct posiciOn * anterior){
	struct mensaje msg;
	long coord;
	struct posiciOn sig;
	if((pos.x == 33 ) && (pos.y == 6)){
		//Semaforo C1 Arriba 		
		if((semaforo_bloqueado(semaforo,C1)) == -1) kill(0, SIGTERM);
		if((semaforo_bloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM);
		//Bloqueamos el semaforo y el cruce
		coord = 1 + pos.y*100 + (pos.x);	//634
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + pos.y*100 + (pos.x + 1);	//635
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + pos.y*100 + (pos.x + 2);	//636
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + pos.y*100 + (pos.x + 3);	//637
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + pos.y*100 + (pos.x + 4);	//638
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		sig = CRUCE_avanzar_coche(pos);
		if((semaforo_desbloqueado(semaforo,C1)) == -1) kill(0, SIGTERM); 
		//Desbloqueamos semaforo C1 una vez que ha avanzado
	}else if((pos.x == 13) && (pos.y == 10)){
		//Semaforo C2 Izquierda
		//Bloqueamos el semaforo 
		if((semaforo_bloqueado(semaforo,C2)) == -1) kill(0, SIGTERM); 	
		coord = 1 + (pos.y)*100 + (pos.x + 5);	//1019
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + (pos.y-1)*100 + (pos.x + 5);	//919
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + (pos.y-2)*100 + (pos.x + 5);	//819
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + (pos.y)*100 + (pos.x + 6);	//1020
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + (pos.y-1)*100 + (pos.x + 6);	//920
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		coord = 1 + (pos.y-2)*100 + (pos.x + 6);	//820
		if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		sig = CRUCE_avanzar_coche(pos);
		if((semaforo_desbloqueado(semaforo,C2)) == -1) kill(0, SIGTERM);
		//Desbloqueamos el semaforo una vez realizado el movimiento 
	}else{
		if(pos.x == 23 && pos.y == 10){
			//Coches de izquierda a derecha-abajo
			//Posiciones de inicio del cruce
			if((semaforo_bloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM);
			//Bloqueamos el cruce
		}
		//Recibe mensajes posicion siguiente
		else if(pos.x == 33 && pos.y == 10 && anterior->y == 10 && anterior->x == 31){
				//Coches de izquierda a derecha-abajo
				//Si ha entrado en el giro de horizontal a vertical
				coord = 1 + (pos.y + 1)*100 + (pos.x);	//1134
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y + 1)*100 + (pos.x + 1);	//1135
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y + 1)*100 + (pos.x + 2);	//1136
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y + 1)*100 + (pos.x + 3);	//1137
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y + 1)*100 + (pos.x + 4);	//1138
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
		}else if(pos.x < 33 && pos.y == 10){
				//Coches de izquierda a derecha-abajo
				//Si está en la horizontal antes del giro
				coord = 1 + (pos.y)*100 + (pos.x + 5);	//1003-1035
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y-1)*100 + (pos.x + 5);	//903-935
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y-2)*100 + (pos.x + 5);	//803-835
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y)*100 + (pos.x + 6);	//1004-1036
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y-1)*100 + (pos.x + 6);	//904-936
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
				coord = 1 + (pos.y-2)*100 + (pos.x + 6);	//804-836
				if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);				
		}else if(pos.y <= 16){
			//Vertical
			//Hasta 16 porque a partir de ahi el coche empieza a desaparecer
			coord = 1 + pos.y*100 + (pos.x);	//134-234-334 ... - 1634
			if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);				
			coord = 1 + pos.y*100 + (pos.x + 1);	//135-235-335 ... - 1635
			if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);		
			coord = 1 + pos.y*100 + (pos.x + 2);	//136-236-336 ... - 1636
			if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);		
			coord = 1 + pos.y*100 + (pos.x + 3);	//137-237-337 ... - 1637 
			if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);		
			coord = 1 + pos.y*100 + (pos.x + 4);	//138-238-338 ... - 1638
			if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);		
		}
		sig = CRUCE_avanzar_coche(pos);
	}	
	if(pos.x == 33 && pos.y == 13){
		//Coordenadas salida cruce
		if((semaforo_desbloqueado(semaforo,CRUCE)) == -1) kill(0, SIGTERM);
		//Desbloqueamos semaforo cruce	
	}
	if((anterior->x != -1)  && (anterior->y != -1)){
	//Si posicion anterior valida es decir no acaba de nacer
		if(pos.x == 33 && pos.y == 10 && anterior->y == 10 && anterior->x == 31){
			//Giro de horizontal a vertical
			msg.coord = 1 + (anterior->y - 2)*100 + (anterior->x);	 //832
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 1)*100 + (anterior->x);	//932
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y)*100 + (anterior->x);		//1032
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 2)*100 + (anterior->x + 1);	//833
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 1)*100 + (anterior->x + 1);	//933
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y)*100 + (anterior->x + 1);	//1033
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		}else if(anterior->x == 1){
			//Comienzo
			msg.coord = 1 + (anterior->y - 2)*100 + (anterior->x + 1);	//803
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 1)*100 + (anterior->x + 1);	//903
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y)*100 + (anterior->x + 1);	//1003
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		}else if(anterior->x <= 31 && anterior->x >= 3){
			//Horizontal
			msg.coord = 1 + (anterior->y - 2)*100 + (anterior->x);	//804-832
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 1)*100 + (anterior->x);	//904-932
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y)*100 + (anterior->x);		//1004-1032
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 2)*100 + (anterior->x + 1);	//805-833
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y - 1)*100 + (anterior->x + 1);	//905-933
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
			msg.coord = 1 + (anterior->y)*100 + (anterior->x + 1);	//1005-1033
			if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 }else if(anterior->y > 4 && anterior->x >31 && pos.y < 21){
		 	//Si está en la vertical y aún no está desapareciendo
		 	msg.coord = 1 + (anterior->y - 4)*100 + (anterior->x + 4);//138,238-1538
		 	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 	msg.coord = 1 + (anterior->y - 4)*100 + (anterior->x + 3);//137,237-1537
		 	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 	msg.coord = 1 + (anterior->y - 4)*100 + (anterior->x + 2);//136,236-1536
		 	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 	msg.coord = 1 + (anterior->y - 4)*100 + (anterior->x + 1);//135,235-1535
		 	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 	msg.coord = 1 + (anterior->y - 4)*100 + (anterior->x);//134,234-1534
		 	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		 }
	}	
	anterior->x = pos.x;
	anterior->y = pos.y;	
	return sig;	
}

void coche(){
	struct posiciOn pos, anterior = {-1,-1};
	struct mensaje msg;
	pos = CRUCE_inicio_coche();
	while(1){
		pos = mover_coche(pos, &anterior);
		if(!pos_valida(pos)) break;
		pausa_coche();
	}
	CRUCE_fin_coche();
		//Ultimas coordenadas
		msg.coord = 1 + (anterior.y - 4)*100 + (anterior.x + 4);//1638
		if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);	
		msg.coord = 1 + (anterior.y - 4)*100 + (anterior.x + 3);//1637
		if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		msg.coord = 1 + (anterior.y - 4)*100 + (anterior.x + 2);//1636
		if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		msg.coord = 1 + (anterior.y - 4)*100 + (anterior.x + 1);//1635
		if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
		msg.coord = 1 + (anterior.y - 4)*100 + (anterior.x);//1634
		if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
	muerte();
}

int pos_valida(struct posiciOn pos){
	if(pos.y < 0) return 0;
	else return 1;
}


struct posiciOn mover_peaton(struct posiciOn pos, struct posiciOn * anterior){
	struct posiciOn sig;
	struct mensaje msg, msg_ant;
	static int critica = 1;
	long coord;
	coord = 1 + pos.x + pos.y*100;
	if(((anterior->y == 16 && anterior->x < 30)  || (anterior->x == 0 && anterior->y <= 16 && anterior->y > 11)) && (critica == 0)){
		//Zona de Spawn de los peatones
		//Bloqueamos semaforo nacimientos
		if((semaforo_bloqueado(semaforo,NACER)) == -1) kill(0, SIGTERM); 
		critica = 1;
	}else if ((anterior->y <= 15  && (anterior->x > 0)) && (critica == 1)){
		//Desbloqueamos semaforo nacimientos
		if((semaforo_desbloqueado(semaforo,NACER)) == -1) kill(0, SIGTERM); 
		critica = 0;	
	}
	//Recibimos mensaje de las coordenadas
	if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
	
	if((pos.x < 28) && (pos.x > 20) && (pos.y == 11)){ 
		//Enfrente de P2
		if((semaforo_bloqueado(semaforo,P2)) == -1) kill(0, SIGTERM); 
		sig = CRUCE_avanzar_peatOn(pos);
		if((semaforo_desbloqueado(semaforo,P2)) == -1) kill(0, SIGTERM); 
	}else if((pos.y < 16) && (pos.y > 12) && (pos.x == 30)){ 
		//Enfrente de P1
		if((semaforo_bloqueado(semaforo,P1)) == -1) kill(0, SIGTERM);
		sig = CRUCE_avanzar_peatOn(pos);
		if((semaforo_desbloqueado(semaforo,P1)) == -1) kill(0, SIGTERM);
	}else{
		sig = CRUCE_avanzar_peatOn(pos);
	}
	
	if((anterior->x != -1)  && (anterior->y != -1)){
		//Enviamos mensajes de las coordenadas de la ultima posicion anterior ocupada
		msg_ant.coord = 1 + anterior->x + anterior->y*100;
		if(msgsnd(buzon, &msg_ant, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM); 	
	}
	anterior->x = pos.x;
	anterior->y = pos.y;
	return sig;
}

void peaton(){
	struct posiciOn pos,anterior = {-1,-1};
	struct mensaje msg;
	long coord;
	if((semaforo_bloqueado(semaforo,NACER)) == -1) kill(0, SIGTERM); 
	pos = CRUCE_inicio_peatOn_ext(&anterior);
	coord = 1 + anterior.y*100 + anterior.x;
	if(msgrcv(buzon, &msg, sizeof(mensaje), coord, MSG_NOERROR) == -1) kill(0, SIGTERM);
	while(1){
		pos = mover_peaton(pos, &anterior);
		if(!pos_valida(pos)) break;
		pausa();
	}
	msg.coord = 1 + anterior.x + anterior.y*100;
	CRUCE_fin_peatOn();
	//Enviamos mensaje de las coordenadas ultima posicion anterior ocupada
	if(msgsnd(buzon, &msg, sizeof(mensaje) - sizeof(long), 0) == -1) kill(0, SIGTERM);
	muerte();
}
//Utilidades semaforos
//Establecer un valor al semaforo
int semaforo_set_valor(int sem,unsigned short i,int val){
  union semun param;
  int a;
  param.val = val;
  a = semctl(sem,i,SETVAL,param);
  if(a == -1){
	perror("Error");
	exit(1);
  }
  return a;
}
//Cambiar valores del semaforo de forma automatica
int semaforo_cambiar_valor(int sem,unsigned short i,int valor){
  static struct sembuf sops = {0,0,0};
  sops.sem_num = i;
  sops.sem_op = valor;
  int e = semop(sem,&sops,1);
  if(e == -1)
  	perror("Error");
  return e;
}
//Wait
int semaforo_bloqueado(int sem,unsigned short i){
 return semaforo_cambiar_valor(sem,i,-1);
}
//Signal
int semaforo_desbloqueado(int sem,unsigned short i){
 return semaforo_cambiar_valor(sem,i,+1);
}
//FUNCIÓN MANEJADORA CTRL+C
void matar(int signal)  {
       int k;
       int cod;
       char pantalla[100];
       int fin;
       fin = CRUCE_fin(); 
       if(getpid() != ppid){
       		kill(getpid(),SIGKILL);      
       } 
       else  {
          for(k = 0; k < max_proc; k++){
                       wait(&cod);
          }	      
        //Borrar semáforo
        if(semctl(semaforo, 0, IPC_RMID) !=0)
        	perror("\nError al borrar el semaforo");	
        
        //Borrar buzón
        if(msgctl(buzon, IPC_RMID, NULL) !=0)
        	perror("Error al borrar el buzon");	
        	
       //Borrar memoria compartida
      	 if(shmdt(puntero_mem) != 0)
      	 	perror("Error al borrar puntero"); //Borrar puntero +256 
        if(shmctl(mcompartida, IPC_RMID, NULL) !=0)
        	perror("Error al borrar la memoria compartida");
	exit(0);       	           
       }
}
