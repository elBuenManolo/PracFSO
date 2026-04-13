/*****************************************************************************/
/* */
/* mur0.c                                                                    */
/* */
/* Programa inicial d'exemple per a les practiques 2 d'FSO.                  */
/* Versió seqüencial adaptada a winsuport2 i memòria compartida IPC.         */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsuport2.h"
#include "memoria.h"
#include <unistd.h>

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
#define FRNTCHAR 'A'
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
int retard;			  /* valor del retard de moviment, en mil.lisegons */
char strin[LONGMISS]; /* variable per a generar missatges de text a la pantalla */

/* Variables de la paleta */
int f_pal, c_pal;  /* posicio del primer caracter de la paleta (fila, columna) */
int m_pal;		   /* mida de la paleta (en caracters) */
int dirPaleta = 0; /* direcció de moviment de la paleta */

/* Variables de la pilota */
int f_pil, c_pil;	/* posicio de la pilota, en valor enter (per pintar a pantalla) */
float pos_f, pos_c; /* posicio real de la pilota, en valor real (per a moviments suaus) */
float vel_f, vel_c; /* velocitat de la pilota (components horitzontal i vertical) */

/* Variables globals per a la memòria compartida (IPC) */
int id_mem;	 /* identificador de la memòria compartida creada */
void *p_mem; /* punter cap a la zona de memòria mapejada */

int minuts, segons;
int comptador_retard = 0;

typedef struct
{
	int nblocs;
	int npilotes;
	char tauler;
} dades_t;

dades_t *comp;

/* * Llegeix els paràmetres del joc des d'un fitxer de text.
 * Retorna 0 si tot va bé, o un codi d'error (1-5) si algun paràmetre és incorrecte.
 */
int carrega_configuracio(FILE *fit)
{
	int ret = 0;

	fscanf(fit, "%d %d %d\n", &n_fil, &n_col, &m_por);
	fscanf(fit, "%d %d\n", &c_pal, &m_pal);
	fscanf(fit, "%f %f %f %f\n", &pos_f, &pos_c, &vel_f, &vel_c);

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

	if (c_pal != 0 && m_pal != 0)
	{
		if ((m_pal < 1) || (m_pal > n_col - 3) || (c_pal < 1) || (c_pal + m_pal > n_col - 1))
			ret = 5;
	}

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

/* * Captura les entrades del teclat de l'usuari i desplaça la paleta.
 * Retorna 1 si es prem la tecla RETURN per abandonar la partida.
 */
int mou_paleta(void)
{
	int tecla, result;

	result = 0;
	tecla = win_gettec();
	if (tecla != 0)
	{
		if ((tecla == TEC_DRETA) && ((c_pal + m_pal) < n_col - 1))
		{
			/* Esborrar l'extrem esquerre i pintar el nou extrem dret */
			win_escricar(f_pal, c_pal, ' ', NO_INV);
			c_pal++;
			win_escricar(f_pal, c_pal + m_pal - 1, '0', INVERS);
		}
		if ((tecla == TEC_ESQUER) && (c_pal > 1))
		{
			/* Esborrar l'extrem dret i pintar el nou extrem esquerre */
			win_escricar(f_pal, c_pal + m_pal - 1, ' ', NO_INV);
			c_pal--;
			win_escricar(f_pal, c_pal, '0', INVERS);
		}
		if (tecla == TEC_RETURN)
			result = 1; /* L'usuari vol sortir */
		dirPaleta = tecla;
	}
	return (result);
}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
	int i, fi1 /*, fi2*/;
	FILE *fit_conf;

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

	char s_id_mem[10], s_fil[10], s_col[10], s_retard[10];
	char s_pos_f[10], s_pos_c[10], s_vel_f[10], s_vel_c[10];
	char s_c_pal[10], s_m_pal[10];

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

	if (fork() == 0)
	{
		/* Passem els arguments com a cadenes de text */
		execlp("./pilota1", "pilota1", s_id_mem, s_fil, s_col,
			   s_pos_f, s_pos_c, s_vel_f, s_vel_c, s_retard, s_c_pal, s_m_pal, (char *)"1", (char *)NULL);
		exit(1);
	}

	/* 4. Bucle principal d'execució del joc */
	do
	{
		fi1 = mou_paleta(); /* Moure la paleta i llegir teclat */
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
		sprintf(strin, "Temps: %02d:%02d | Blocs: %d | Pilotes: %d", minuts, segons, comp->nblocs, comp->npilotes);
		win_escristr(strin);

		win_update();		/* Bolcar els canvis fets a la memòria a la pantalla física */
		win_retard(retard); /* Pausar el procés el temps establert abans del següent frame */
	} while (!fi1 && comp->nblocs > 0 && comp->npilotes > 0); /* Sortir si demanem sortir (!fi1) o acaba la partida (!fi2) */

	/* 5. Comprovació de final de joc i missatges de sortida */
	if (comp->nblocs == 0)
		mostra_final("YOU WIN !");
	else
		mostra_final("GAME OVER");

	win_fi();

	printf("Temps de joc -> %02d:%02d\n", minuts, segons);
	/* 6. Alliberament obligatori de la memòria compartida creada a l'inici */
	elim_mem(id_mem);

	return (0);
}
