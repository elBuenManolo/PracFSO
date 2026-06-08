/*****************************************************************************/
/* */
/* mur4.c                                                                    */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsuport2.h"
#include "semafor.h"
#include "memoria.h"
#include "missatge.h"
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>

/* --- Definicions de constants --- */
#define MAX_THREADS 10
#define MAXBALLS (MAX_THREADS - 1)
#define MIN_FIL 10
#define MAX_FIL 50
#define MIN_COL 10
#define MAX_COL 80

/* Constants per a la creació dels blocs del joc */
#define BLKSIZE 3
#define BLKGAP 2
#define BLKCHAR 'B'
#define WLLCHAR '#'
#define FRNTCHAR 'T'
#define LONGMISS 65

/* Text d'ajuda que es mostra si s'executa el programa sense arguments */
char *descripcio[] = {
	"\n",
	"Aquest programa implementa una versio basica del joc Arkanoid o Breakout:\n",
	"generar un camp de joc rectangular amb una porteria, una paleta que s\'ha\n",
	"de moure amb el teclat per a cobrir la porteria, i una pilota que rebota\n",
	"contra les parets del camp, a la paleta i els blocs. El programa acaba si\n",
	"la pilota surt per la porteria o no queden mes blocs. Tambe es pot acabar\n",
	"amb la tecla RETURN.\n",
	"\n",
	"  Arguments del programa:\n",
	"\n",
	"       $ ./mur0 fitxer_config [retard]\n",
	"\n",
	"     El primer argument ha de ser el nom d\'un fitxer de text amb la\n",
	"     configuracio de la partida, on la primera fila inclou informacio\n",
	"     del camp de joc (enters), la segona fila indica posicio i mida\n",
	"     de la paleta (enters) i la tercera fila indica posicio\n",
	"     i velocitat de la pilota (valors reals):\n",
	"          num_files  num_columnes  mida_porteria\n",
	"          pos_col_paleta  mida_paleta\n",
	"          pos_fila   pos_columna   vel_fila  vel_columna\n",
	"\n",
	"     Alternativament, es pot donar el valor 0 per especificar configuracio\n",
	"     automatica (pantalla completa, porteria calculada, paleta al mig, etc).\n",
	"*"};

/* --- Variables Globals --- */
/* Variables de l'entorn de joc */
int n_fil, n_col;	  /* dimensions del camp de joc */
int m_por;			  /* mida de la porteria (en caracters) */
int npilotes;		  /* nombre de pilotes */
int retard;			  /* valor del retard de moviment, en mil.lisegons */
char strin[LONGMISS]; /* variable per a generar missatges de text a la pantalla */

/* Variables de la paleta */
int f_pal, c_pal;  /* posicio del primer caracter de la paleta (fila, columna) */
int m_pal;		   /* mida de la paleta (en caracters) */
int dirPaleta = 0; /* direcció de moviment de la paleta */
int npaletes;

/* Variables de la pilota */
int f_pil, c_pil;	/* posicio de la pilota, en valor enter (per pintar a pantalla) */
float pos_f, pos_c; /* posicio real de la pilota, en valor real (per a moviments suaus) */
float vel_f, vel_c; /* velocitat de la pilota (components horitzontal i vertical) */

/* Variables globals per a la memòria compartida (IPC) */
int id_mem; /* identificador de la memòria compartida creada */
int id_sem;
int id_mis;

int minuts, segons;
int comptador_retard = 0;

int tecla_global = 0;
int ultima_moguda;
bool stop = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    char origen;
    char desti;
    char s_id_mem[8], s_fil[8], s_col[8], s_retard[8];
    char s_pos_f[8], s_pos_c[8], s_vel_f[8], s_vel_c[8];
    char s_c_pal[8], s_m_pal[8], s_id_sem[8], s_id_mis[8];
	int velf_pilota, velc_pilota;
} MISSATGE;

typedef struct
{
	int nblocs;
	int npilotes;
	int fi1;
	int poder_actiu; // Flag booleà per saber si el poder està actiu
	bool stop;
	char tauler;
} dades_t;

typedef struct
{
	int f_pal, c_pal;
	int m_pal;
	int dirPaleta;
} paleta_t;

pthread_t tid[MAX_THREADS + 1];

dades_t *comp;
paleta_t paletes[MAX_THREADS];

/* * Llegeix els paràmetres del joc des d'un fitxer de text.
 * Retorna 0 si tot va bé, o un codi d'error (1-5) si algun paràmetre és incorrecte.
 */
int carrega_configuracio(FILE *fit)
{
	int ret = 0;

	fscanf(fit, "%d %d %d\n", &n_fil, &n_col, &m_por);
	fscanf(fit, "%d %d\n", &c_pal, &m_pal);
	fscanf(fit, "%f %f %f %f\n", &pos_f, &pos_c, &vel_f, &vel_c);
	
	fscanf(fit, "P %d\n", &npaletes);			// La primera fila amb P és el nombre de paletes
	
	if (npaletes > 1){
		for (int i = 0; i < npaletes - 1; i++)
		{
			fscanf(fit, "%d %d %d %d\n", &paletes[i].f_pal, &paletes[i].c_pal, &paletes[i].m_pal, &paletes[i].dirPaleta);
			
			if (paletes[i].c_pal != 0 && paletes[i].m_pal != 0)
			{
				if ((paletes[i].m_pal < 1) || (paletes[i].m_pal > n_col - 3) || (paletes[i].c_pal < 1) || (paletes[i].c_pal + paletes[i].m_pal > n_col - 1))
					ret = 5;
			}
		}
	}

	/* Validació de les dimensions i posicions per evitar errors gràfics */
	if ((n_fil != 0) || (n_col != 0))
	{
		if ((n_fil < MIN_FIL) || (n_fil > MAX_FIL) || (n_col < MIN_COL) || (n_col > MAX_COL))
			ret = 1;
		else if (m_por > n_col - 3)
			ret = 2;
		else if ((pos_f < 1) || (pos_f >= n_fil - 3) || (pos_c < 1) || (pos_c > n_col - 1))
			ret = 3;
	}
	if ((vel_f < -1.0) || (vel_f > 1.0) || (vel_c < -1.0) || (vel_c > 1.0))
		ret = 4;


	if (npaletes > MAX_THREADS || npaletes < 1)
		ret = 6;

	if (ret != 0)
	{
		fprintf(stderr, "Error en fitxer de configuracio:\n");
		switch (ret)
		{
		case 1:
			fprintf(stderr, "\tdimensions del camp de joc incorrectes\n");
			break;
		case 2:
			fprintf(stderr, "\tmida de la porteria incorrecta\n");
			break;
		case 3:
			fprintf(stderr, "\tposicio de la pilota incorrecta\n");
			break;
		case 4:
			fprintf(stderr, "\tvelocitat de la pilota incorrecta\n");
			break;
		case 5:
			fprintf(stderr, "\tposicio o mida de la paleta incorrectes\n");
			break;
		case 6:
			fprintf(stderr, "\tnumero de paletes incorrecte\n");
			break;
		}
	}
	fclose(fit);
	return (ret);
}

/* * Prepara la memòria compartida, dibuixa els elements del joc (murs, blocs,
 * paleta, pilota) i inicialitza les variables automàtiques si s'escau.
 */
int inicialitza_joc(void)
{
	int i, mida_tauler, mida_total;
	int i_port, f_port;
	int c, nb, offset;

	/* win_ini retorna la mida necessària de memòria per la configuració actual */
	mida_tauler = win_ini(&n_fil, &n_col, '+', INVERS);

	if (mida_tauler < 0)
	{
		fprintf(stderr, "Error en la creacio del taulell de joc.\n");
		return (mida_tauler);
	}

	/* Creació i assignació de la memòria compartida per a la pantalla */
	mida_total = mida_tauler + sizeof(dades_t);
	id_mem = ini_mem(mida_total);
	comp = (dades_t *)map_mem(id_mem);
	win_set(&(comp->tauler), n_fil, n_col);

	comp->npilotes = 0;
	comp->poder_actiu = 0;

	/* Càlcul de la porteria inferior */
	if (m_por > n_col - 2)
		m_por = n_col - 2;
	if (m_por == 0)
		m_por = 3 * (n_col - 2) / 4;
	i_port = n_col / 2 - m_por / 2 - 1;
	f_port = i_port + m_por - 1;
	for (i = i_port; i <= f_port; i++)
		win_escricar(n_fil - 2, i, ' ', NO_INV);

	n_fil = n_fil - 1;
	f_pal = n_fil - 2;

	/* Mode automàtic per a la paleta (si al fitxer s'ha passat valor 0) */
	if (m_pal == 0)
		m_pal = m_por / 2;
	if (m_pal < 1)
		m_pal = 1;
	if (c_pal == 0)
		c_pal = (n_col - m_pal) / 2;

	/* Dibuixar la paleta a la pantalla */
	for (i = 0; i < m_pal; i++)
		win_escricar(f_pal, c_pal + i, '0', INVERS);

	for (i = 0; i < npaletes - 1; i++){
		for (int j = 0; j < paletes[i].m_pal; j++){
			win_escricar(paletes[i].f_pal, paletes[i].c_pal + j, (char)((i + 1) + '0'), INVERS);
		}
	}

	/* Ubicar i dibuixar la pilota a la posició inicial */
	if (pos_f > n_fil - 1)
		pos_f = n_fil - 1;
	if (pos_c > n_col - 1)
		pos_c = n_col - 1;
	f_pil = pos_f;
	c_pil = pos_c;

	/* Generació dels blocs a destruir (files 3, 4 i 5) */
	nb = 0;
	comp->nblocs = n_col / (BLKSIZE + BLKGAP) - 1;
	offset = (n_col - comp->nblocs * (BLKSIZE + BLKGAP) + BLKGAP) / 2;
	for (i = 0; i < comp->nblocs; i++)
	{
		for (c = 0; c < BLKSIZE; c++)
		{
			win_escricar(3, offset + c, FRNTCHAR, INVERS);
			nb++;
			win_escricar(4, offset + c, BLKCHAR, NO_INV);
			nb++;
			win_escricar(5, offset + c, FRNTCHAR, INVERS);
			nb++;
		}
		offset += BLKSIZE + BLKGAP;
	}
	comp->nblocs = nb / BLKSIZE;

	/* Generació de les defenses indestructibles (fila 6) */
	nb = n_col / (BLKSIZE + 2 * BLKGAP) - 2;
	offset = (n_col - nb * (BLKSIZE + 2 * BLKGAP) + BLKGAP) / 2;
	for (i = 0; i < nb; i++)
	{
		for (c = 0; c < BLKSIZE + BLKGAP; c++)
		{
			win_escricar(6, offset + c, WLLCHAR, NO_INV);
		}
		offset += BLKSIZE + 2 * BLKGAP;
	}

	minuts = 0;
	segons = 0;

	sprintf(strin, "Tecles: \'%c\'-> Esquerra, \'%c\'-> Dreta, RETURN-> sortir\n", TEC_DRETA, TEC_ESQUER);
	win_escristr(strin);
	return (0);
}

/* * Mostra el missatge final de partida a la línia d'estat i espera a que
 * l'usuari premi una tecla per tancar l'aplicació.
 */
void mostra_final(char *miss)
{
	int lmarge;
	char marge[LONGMISS];

	/* Centra el text calculant el marge necessari */
	lmarge = (n_col + strlen(miss)) / 2;
	sprintf(marge, "%%%ds", lmarge);

	sprintf(strin, marge, miss);
	win_escristr(strin);
	win_update();
	getchar();
}

/* * Calcula l'efecte de la pilota depenent d'on impacti sobre la paleta.
 * Si pica a les vores, el rebot és més inclinat.
 */
float control_impacte2(int c_pil, float velc0)
{
	int distApal;
	float vel_c;
	

	distApal = c_pil - c_pal;
	if (distApal >= 2 * m_pal / 3)
		vel_c = 0.5;
	else if (distApal <= m_pal / 3)
		vel_c = -0.5;
	else if (distApal == m_pal / 2)
		vel_c = 0.0;
	else
		vel_c = velc0;
	return vel_c;
}

void * mou_paleta(void * arg){

	int num_paleta = (int)(long) arg;
	// 0 = Paleta de l'usuari
	if (num_paleta == 0){

		do{
			while(stop){
			win_retard(retard);
			}
			if (tecla_global != 0)
			{
				if ((tecla_global == TEC_DRETA) && ((c_pal + m_pal) < n_col - 1))
				{
					/* Esborrar l'extrem esquerre i pintar el nou extrem dret */
					waitS(id_sem);
					win_escricar(f_pal, c_pal, ' ', NO_INV);
					signalS(id_sem);
					c_pal++;
					waitS(id_sem);
					win_escricar(f_pal, c_pal + m_pal - 1, '0', INVERS);
					signalS(id_sem);
				}
				if ((tecla_global == TEC_ESQUER) && (c_pal > 1))
				{
					/* Esborrar l'extrem dret i pintar el nou extrem esquerre */
					waitS(id_sem);
					win_escricar(f_pal, c_pal + m_pal - 1, ' ', NO_INV);
					signalS(id_sem);
					c_pal--;
					waitS(id_sem);
					win_escricar(f_pal, c_pal, '0', INVERS);
					signalS(id_sem);
				}
				if (tecla_global == TEC_RETURN){
					waitS(id_sem);
					comp->fi1 = 1; /* L'usuari vol sortir */
					signalS(id_sem);
				}
				dirPaleta = tecla_global;
			}
			
			win_retard(retard);
		} while (!comp->fi1 && comp->nblocs > 0 && comp->npilotes > 0);

	}
	else{
		int possible = -1; // Per defecte cap a dalt (-1 disminueix la fila)
		int pal = num_paleta - 1;			// Com paletes[] està en base 0 i dins de paletes no està la paleta de l'usuari, restem 1 per accedir correctament
		do{
			while(stop){
			win_retard(retard);
			}

			MISSATGE msg_rebut;
            if (receiveM(id_mis, &msg_rebut) > 0) {
                // Comprovem si el missatge va dirigit a AQUESTA paleta
                if (msg_rebut.desti == (num_paleta + '0')) {
                    
                    // Creem el missatge sol·licitant al Main (desti 'P') la nova pilota
                    MISSATGE msg_spawn;
                    msg_spawn.origen = 'F'; // 'F' és el que espera el Main per crear pilotes
                    msg_spawn.desti = 'P';  // Procés principal

                    // Informació general (convertida a string per l'execlp)
                    sprintf(msg_spawn.s_id_mem, "%d", id_mem);
                    sprintf(msg_spawn.s_fil, "%d", n_fil);
                    sprintf(msg_spawn.s_col, "%d", n_col);
                    sprintf(msg_spawn.s_retard, "%d", retard);

                    // POSICIÓ I VELOCITAT (a sobre del punt mig, V=-1, H=0)
                    pthread_mutex_lock(&mutex);
                    float nova_pos_f = (float)paletes[pal].f_pal - 1.0;
                    float nova_pos_c = (float)paletes[pal].c_pal + ((float)paletes[pal].m_pal / 2.0);
                    pthread_mutex_unlock(&mutex);

                    sprintf(msg_spawn.s_pos_f, "%.2f", nova_pos_f);
                    sprintf(msg_spawn.s_pos_c, "%.2f", nova_pos_c);
                    sprintf(msg_spawn.s_vel_f, "-1.00");
                    sprintf(msg_spawn.s_vel_c, "0.00");

                    // Dades de la paleta usuari (per a rebots del joc)
                    sprintf(msg_spawn.s_c_pal, "%d", c_pal);
                    sprintf(msg_spawn.s_m_pal, "%d", m_pal);
                    sprintf(msg_spawn.s_id_sem, "%d", id_sem);
                    sprintf(msg_spawn.s_id_mis, "%d", id_mis);

                    // Enviem la petició definitiva al main
                    sendM(id_mis, &msg_spawn, sizeof(MISSATGE));
                } else {
                    // Si el missatge no és per a mi, el retorno a la cua de missatges
                    sendM(id_mis, &msg_rebut, sizeof(MISSATGE));
                }
            }

			if (tecla_global == (num_paleta + '0')){

				ultima_moguda = num_paleta;

				for (int i = 0; i < paletes[pal].m_pal; i++){
					waitS(id_sem);
					if (possible == 1 && win_quincar(paletes[pal].f_pal + 1, paletes[pal].c_pal + i) != ' '){
						possible = -1; // Si xoca a baix, cap a dalt
					}
					else if (possible == -1 && win_quincar(paletes[pal].f_pal - 1, paletes[pal].c_pal + i) != ' '){
						possible = 1; // Si xoca a dalt, cap a baix
					}
					signalS(id_sem);
				}
				

				if (possible != 0){
					for (int i = 0; i < paletes[pal].m_pal; i++){
						waitS(id_sem);
						win_escricar(paletes[pal].f_pal, paletes[pal].c_pal + i, ' ', NO_INV);
						signalS(id_sem);
					}

					for (int i = 0; i < paletes[pal].m_pal; i++){
						waitS(id_sem);
						win_escricar(paletes[pal].f_pal + possible, paletes[pal].c_pal + i, (char)(num_paleta + '0'), INVERS);
						signalS(id_sem);
					}
					pthread_mutex_lock(&mutex);
					paletes[pal].f_pal += possible; 
					pthread_mutex_unlock(&mutex);
				}
			}

			if (paletes[pal].dirPaleta == 1) // Dreta
			{
				if ((paletes[pal].c_pal + paletes[pal].m_pal) < n_col - 1)
				{
					/* Esborrar l'extrem esquerre i pintar el nou extrem dret */
					waitS(id_sem);
					win_escricar(paletes[pal].f_pal, paletes[pal].c_pal, ' ', NO_INV);
					signalS(id_sem);
					pthread_mutex_lock(&mutex);
					paletes[pal].c_pal++; 
					pthread_mutex_unlock(&mutex);
					waitS(id_sem);
					win_escricar(paletes[pal].f_pal, paletes[pal].c_pal + paletes[pal].m_pal - 1, (char)(num_paleta + '0'), INVERS);
					signalS(id_sem);
				}
				else
				{
					pthread_mutex_lock(&mutex);
					paletes[pal].dirPaleta = -1; // Canvia de direcció
					pthread_mutex_unlock(&mutex);
				}
			}
			else if (paletes[pal].dirPaleta == -1) // Esquerra
			{
				if (paletes[pal].c_pal > 1)
				{
					/* Esborrar l'extrem dret i pintar el nou extrem esquerre */
					waitS(id_sem);
					win_escricar(paletes[pal].f_pal, paletes[pal].c_pal + paletes[pal].m_pal - 1, ' ', NO_INV);
					signalS(id_sem);
					pthread_mutex_lock(&mutex);
					paletes[pal].c_pal--; 
					pthread_mutex_unlock(&mutex);
					waitS(id_sem);
					win_escricar(paletes[pal].f_pal, paletes[pal].c_pal, (char)(num_paleta + '0'), INVERS);
					signalS(id_sem);
				}
				else
				{
					pthread_mutex_lock(&mutex);
					paletes[pal].dirPaleta = 1;  // Canvia de direcció
					pthread_mutex_unlock(&mutex);
				}
			}
			win_retard(retard);
		} while (!comp->fi1 && comp->nblocs > 0 && comp->npilotes > 0); 

	}
	return NULL;
}

void crear_pilota(MISSATGE *missatge) {
	if (fork() == 0){
		char s_npilotes[8];
		sprintf(s_npilotes, "%d", npilotes);
		execlp("./pilota4", "pilota4",
			missatge->s_id_mem,
			missatge->s_fil,
			missatge->s_col,
			missatge->s_pos_f,
			missatge->s_pos_c,
			missatge->s_vel_f,
			missatge->s_vel_c,
			missatge->s_retard,
			missatge->s_c_pal,
			missatge->s_m_pal,
			s_npilotes,
			missatge->s_id_sem,
			missatge->s_id_mis,
			(char *)NULL);
		exit(1); // Surto del fill, el pare no ha de continuar
	}
	npilotes++;
}



/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
	int i;

	FILE *fit_conf;
	int temps_poder = 0; // Variable local per al temporitzador del poder

	/* 1. Comprovació d'arguments d'entrada */
	if ((n_args != 2) && (n_args != 3))
	{
		i = 0;
		do
			fprintf(stderr, "%s", descripcio[i++]);
		while (descripcio[i][0] != '*');
		exit(1);
	}

	fit_conf = fopen(ll_args[1], "rt");
	if (!fit_conf)
	{
		fprintf(stderr, "Error: no s'ha pogut obrir el fitxer \'%s\'\n", ll_args[1]);
		exit(2);
	}

	/* 2. Càrrega del fitxer i configuració del retard del joc */
	if (carrega_configuracio(fit_conf) != 0)
		exit(3);

	if (n_args == 3)
	{
		retard = atoi(ll_args[2]);
		if (retard < 10)
			retard = 10;
		if (retard > 1000)
			retard = 1000;
	}
	else
		retard = 100;

	printf("Joc del Mur: prem RETURN per continuar:\n");
	getchar();

	/* 3. Inicialització de la memòria compartida i del taulell gràfic */
	if (inicialitza_joc() != (pid_t)0)
		exit(4);

	id_sem = ini_sem(1);
	id_mis = ini_mis();

	MISSATGE missatge_enviat, missatge_paleta;
	missatge_paleta.origen = 'P';
	missatge_enviat.desti = 'P';
	missatge_enviat.origen = 'P';

	char s_id_mem[8], s_fil[8], s_col[8], s_retard[8];
	char s_pos_f[8], s_pos_c[8], s_vel_f[8], s_vel_c[8];
	char s_c_pal[8], s_m_pal[8], s_id_sem[8], s_id_mis[8];

	/* Conversió de dades a string */
	sprintf(s_id_mem, "%d", id_mem);
	sprintf(s_fil, "%d", n_fil);
	sprintf(s_col, "%d", n_col);
	sprintf(s_retard, "%d", retard);
	sprintf(s_pos_f, "%.2f", pos_f);
	sprintf(s_pos_c, "%.2f", pos_c);
	sprintf(s_vel_f, "%.2f", vel_f);
	sprintf(s_vel_c, "%.2f", vel_c);
	sprintf(s_c_pal, "%d", c_pal);
	sprintf(s_m_pal, "%d", m_pal);
	sprintf(s_id_sem, "%d", id_sem);
	sprintf(s_id_mis, "%d", id_mis);

	if (fork() == 0)
	{

		/* Passem els arguments com a cadenes de text */
		execlp("./pilota4", "pilota4", s_id_mem, s_fil, s_col,
			   s_pos_f, s_pos_c, s_vel_f, s_vel_c, s_retard, s_c_pal, s_m_pal, (char *)"1", s_id_sem, s_id_mis, (char *)NULL);
		exit(1);
	}
	npilotes = 2; // comptador per a escriure

	// 0 = Paleta de l'usuari
	pthread_create(&tid[0], NULL, mou_paleta, (void*)(long)0);

	// Paletes controlades per la IA
	for (int i = 1; i < npaletes; i++){
		pthread_create(&tid[i], NULL, mou_paleta, (void*)(long)i);
	}
	/* 4. Bucle principal d'execució del joc */
	do
	{

		tecla_global = win_gettec();
		MISSATGE missatge_rebut; 

		//tasca nº2 't' CREAR PILOTES
		if (tecla_global == TEC_CREATE && ultima_moguda > 0) {
            MISSATGE msg_teclat;
            msg_teclat.origen = 'M'; // Origen: Main
            msg_teclat.desti = ultima_moguda + '0'; // Destí: Paleta autònoma específica
            sendM(id_mis, &msg_teclat, sizeof(MISSATGE));
        }

        if (receiveM(id_mis, &missatge_rebut) > 0) {
            // El Main nomes processa els missatges que van a 'P' (Principal)
            if (missatge_rebut.desti == 'P') {
                if (missatge_rebut.origen == 'F') { // Petició de crear pilota
                    crear_pilota(&missatge_rebut);
                } else if (missatge_rebut.origen == 'T') { // Petició del poder
                    temps_poder += 5000; 
                    waitS(id_sem);
                    comp->poder_actiu = 1;
                    signalS(id_sem);
                }
            } else {
                sendM(id_mis, &missatge_rebut, sizeof(MISSATGE));
            }
		}

		//tasca nº1 ' ' STOP 
		if(tecla_global == TEC_STOP){
			stop = true;
			comp->stop = true;
			fprintf(stderr, "%d", tecla_global);
		}

		do{
				comptador_retard += retard;
				
				if (comptador_retard >= 1000) /* Ha passat 1 segon */
				{
					segons++;
					if (segons == 60)
					{
						minuts++;
						segons = 0;
					}
					comptador_retard = 0;
				}
			if(stop == true){
				tecla_global = win_gettec();

				waitS(id_sem);
				signalS(id_sem);

				if (temps_poder > 0)
					sprintf(strin, "Temps: %02d:%02d | Blocs: %d | Pilotes: %d | PODER: %.1fs", minuts, segons, comp->nblocs, comp->npilotes, temps_poder / 1000.0);
				else
					sprintf(strin, "Temps: %02d:%02d | Blocs: %d | Pilotes: %d", minuts, segons, comp->nblocs, comp->npilotes);
				
				win_escristr(strin);
				waitS(id_sem);
				win_update(); /* Bolcar els canvis fets a la memòria a la pantalla física */
				signalS(id_sem);
				
				win_retard(retard);
				if (tecla_global == TEC_STOP){
					stop = false;
					waitS(id_sem);
					comp->stop = false;
					signalS(id_sem);
				}
			}
		}
		while(stop);
		
		if (temps_poder > 0) {
			temps_poder -= retard;
			if (temps_poder <= 0) {
				temps_poder = 0;
				waitS(id_sem);
				comp->poder_actiu = 0;
				signalS(id_sem);
			}
		}

		
		if (temps_poder > 0)
			sprintf(strin, "Temps: %02d:%02d | Blocs: %d | Pilotes: %d | PODER: %.1fs", minuts, segons, comp->nblocs, comp->npilotes, temps_poder / 1000.0);
		else
			sprintf(strin, "Temps: %02d:%02d | Blocs: %d | Pilotes: %d", minuts, segons, comp->nblocs, comp->npilotes);
		
		win_escristr(strin);
		waitS(id_sem);
		win_update(); /* Bolcar els canvis fets a la memòria a la pantalla física */
		signalS(id_sem);
		win_retard(retard); /* Pausar el procés el temps establert abans del següent frame */
	
	} while (!comp->fi1 && comp->nblocs > 0 && comp->npilotes > 0); /* Sortir si demanem sortir (!fi1) o acaba la partida (!fi2) */

	/* 5. Comprovació de final de joc i missatges de sortida */
	if (comp->nblocs == 0)
		mostra_final("YOU WIN !");
	else
		mostra_final("GAME OVER");

	win_fi();

	// Això anirà esperant fills fins que ja no en quedi cap
	while (wait(NULL) > 0);

	printf("Temps de joc -> %02d:%02d\n", minuts, segons);
	/* 6. Alliberament obligatori de la memòria compartida creada a l'inici */
	elim_mem(id_mem);
	elim_sem(id_sem);
	elim_mis(id_mis);

	return (0);
}
