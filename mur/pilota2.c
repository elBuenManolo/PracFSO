#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsuport2.h"
#include "memoria.h"
#include <unistd.h>
#include "semafor.h"

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

/* Variables de la pilota */
int f_pil, c_pil;   /* posicio de la pilota, en valor enter (per pintar a pantalla) */
float pos_f, pos_c; /* posicio real de la pilota, en valor real (per a moviments suaus) */
float vel_f, vel_c; /* velocitat de la pilota (components horitzontal i vertical) */

int num_pilota;

int retard;

/* Variables globals per a la memòria compartida (IPC) */
int id_mem; /* identificador de la memòria compartida creada */

int fi2;
int c_pal, m_pal;

int id_sem;

int n_fil;
int n_col;

char numero;

typedef struct
{
    int nblocs;
    int npilotes;
    char tauler;
} dades_t;

dades_t *comp; /* punter cap a la zona de memòria mapejada */

/* * Donada una posició on la pilota ha xocat, comprova si és un bloc de lletres.
 * Si ho és, esborra tot el bloc de la pantalla i redueix el comptador de blocs.
 */
char comprovar_bloc(int f, int c)
{
    int col;

    waitS(id_sem);
    char quin = win_quincar(f, c);
    signalS(id_sem);
    if (quin == BLKCHAR || quin == FRNTCHAR)
    {
        col = c;
        waitS(id_sem);
        /* Esborrar cap a la dreta fins trobar un espai buit */
        while (win_quincar(f, col) != ' ')
        {
            win_escricar(f, col, ' ', NO_INV);
            col++;
        }
        col = c - 1;
        /* Esborrar cap a l'esquerra fins trobar un espai buit */
        while (win_quincar(f, col) != ' ')
        {
            win_escricar(f, col, ' ', NO_INV);
            col--;
        }
        comp->nblocs--; /* Decrementem el total de blocs pendents */
        signalS(id_sem);
        return quin;
    }
    return ' ';
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

/* * Funció principal de moviment de la pilota.
 * Calcula la següent posició i gestiona els rebots amb parets, blocs i paleta.
 * Retorna 1 si s'ha d'acabar el joc (es guanya o es perd), 0 si s'ha de continuar.
 */
int mou_pilota(void)
{
    int f_h, c_h;
    char rh, rv, rd;
    int fora = 0; /* Booleà: indica si la pilota ha caigut per la porteria */

    f_h = pos_f + vel_f;
    c_h = pos_c + vel_c;
    rh = rv = rd = ' ';

    /* Només mirem rebots si canvia la posició visual (enters) de la pilota */
    if ((f_h != f_pil) || (c_h != c_pil))
    {

        /* Comprovar rebot vertical (sostre, paleta, o bloc a dalt/baix) */
        if (f_h != f_pil)
        {
            waitS(id_sem);
            rv = win_quincar(f_h, c_pil);
            signalS(id_sem);
            if (rv != ' ')
            {
                if (comprovar_bloc(f_h, c_pil) == BLKCHAR)
                {
                    char s_id_mem[10], s_fil[10], s_col[10], s_retard[10];
                    char s_pos_f[10], s_pos_c[10], s_vel_f[10], s_vel_c[10];
                    char s_c_pal[10], s_m_pal[10], s_numero[10], s_id_sem[10];

                    float nova_vel_f = -vel_f;
                    float nova_vel_c = -vel_c;

                    sprintf(s_id_mem, "%d", id_mem);
                    sprintf(s_fil, "%d", n_fil);
                    sprintf(s_col, "%d", n_col);
                    sprintf(s_retard, "%d", retard);
                    sprintf(s_pos_f, "%.2f", pos_f);
                    sprintf(s_pos_c, "%.2f", pos_c);
                    sprintf(s_vel_f, "%.2f", nova_vel_f);
                    sprintf(s_vel_c, "%.2f", nova_vel_c);
                    sprintf(s_c_pal, "%d", c_pal);
                    sprintf(s_m_pal, "%d", m_pal);
                    sprintf(s_numero, "%d", comp->npilotes);
                    sprintf(s_id_sem, "%d", id_sem);

                    if (fork() == 0)
                    {
                        /* Passem els arguments com a cadenes de text */
                        execlp("./pilota2", "pilota2", s_id_mem, s_fil, s_col,
                               s_pos_f, s_pos_c, s_vel_f, s_vel_c, s_retard, s_c_pal, s_m_pal, s_numero, s_id_sem, (char *)NULL);
                        exit(1);
                    }
                }

                if (rv == '0')
                    vel_c = control_impacte2(c_pil, vel_c);
                vel_f = -vel_f;
                f_h = pos_f + vel_f;
            }
        }
        /* Comprovar rebot horitzontal (parets laterals o costats dels blocs) */
        if (c_h != c_pil)
        {
            waitS(id_sem);
            rh = win_quincar(f_pil, c_h);
            signalS(id_sem);
            if (rh != ' ')
            {
                comprovar_bloc(f_pil, c_h);
                vel_c = -vel_c;
                c_h = pos_c + vel_c;
            }
        }
        /* Comprovar rebot diagonal (caires de les estructures) */
        if ((f_h != f_pil) && (c_h != c_pil))
        {
            waitS(id_sem);
            rd = win_quincar(f_h, c_h);
            signalS(id_sem);
            if (rd != ' ')
            {
                comprovar_bloc(f_h, c_h);
                vel_f = -vel_f;
                vel_c = -vel_c;
                f_h = pos_f + vel_f;
                c_h = pos_c + vel_c;
            }
        }

        waitS(id_sem);
        /* Si l'espai està lliure, moure la pilota i redibuixar */
        if (win_quincar(f_h, c_h) == ' ')
        {
            win_escricar(f_pil, c_pil, ' ', NO_INV);
            pos_f += vel_f;
            pos_c += vel_c;
            f_pil = f_h;
            c_pil = c_h;
            /* Si estem dins del tauler, la pintem. Si passem la línia, s'ha colat */
            if (f_pil != n_fil - 1)
                win_escricar(f_pil, c_pil, numero, INVERS);
            else
            {
                fora = 1;
                comp->npilotes--;
            }
        }
        signalS(id_sem);
    }
    else
    {
        /* Encara que no canviï de quadrat a la pantalla, actualitzem coordenades reals */
        pos_f += vel_f;
        pos_c += vel_c;
    }

    /* Retorna verdader (1) si ja no hi ha blocs o la pilota s'ha colat */
    return (comp->nblocs == 0 || fora);
}

int main(int n_args, char *ll_args[])
{
    if (n_args < 13)
        exit(1);

    id_mem = atoi(ll_args[1]);
    n_fil = atoi(ll_args[2]);
    n_col = atoi(ll_args[3]);
    pos_f = atof(ll_args[4]);
    pos_c = atof(ll_args[5]);
    vel_f = atof(ll_args[6]);
    vel_c = atof(ll_args[7]);
    retard = atoi(ll_args[8]);
    c_pal = atoi(ll_args[9]);
    m_pal = atoi(ll_args[10]);
    numero = (char)ll_args[11][0];
    id_sem = atoi(ll_args[12]);

    comp = map_mem(id_mem);
    win_set(&(comp->tauler), n_fil, n_col);
    comp->npilotes = comp->npilotes + 1;

    do
    {
        fi2 = mou_pilota();
        win_retard(retard);
    } while (!fi2);

    return 0;
}