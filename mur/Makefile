# Makefile per a la Pràctica 2 d'FSO
# Compilació dels programes mur0, mur1 i pilota1

CC = gcc
CFLAGS = -Wall -g
LIBS = -lcurses

# Objectius principals (es compilen tots si fem 'make' a seques)
all: mur0 mur1 pilota1 mur2 pilota2 mur3 pilota3

fase1: mur1 pilota1

fase2: mur2 pilota2

fase3: mur3 pilota3

# Rutines compartides (llibreria gràfica i IPC)
winsuport2.o: winsuport2.c winsuport2.h
	$(CC) $(CFLAGS) -c winsuport2.c -o winsuport2.o

memoria.o: memoria.c memoria.h
	$(CC) $(CFLAGS) -c memoria.c -o memoria.o

semafor.o: semafor.c semafor.h
	$(CC) $(CFLAGS) -c semafor.c -o semafor.o

missatge.o: missatge.c missatge.h
	$(CC) $(CFLAGS) -c missatge.c -o missatge.o

# --- FASE 0 ---
mur0.o: mur0.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c mur0.c -o mur0.o

mur0: mur0.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) mur0.o winsuport2.o memoria.o -o mur0 $(LIBS)

# --- FASE 1 ---
mur1.o: mur1.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c mur1.c -o mur1.o

mur1: mur1.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) mur1.o winsuport2.o memoria.o -o mur1 $(LIBS)

pilota1.o: pilota1.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c pilota1.c -o pilota1.o

pilota1: pilota1.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) pilota1.o winsuport2.o memoria.o -o pilota1 $(LIBS)

# --- FASE 2 ---

mur2.o: mur2.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c mur2.c -o mur2.o

mur2: mur2.o winsuport2.o memoria.o semafor.o missatge.o
	$(CC) $(CFLAGS) mur2.o winsuport2.o memoria.o semafor.o missatge.o -o mur2 $(LIBS)

pilota2.o: pilota2.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c pilota2.c -o pilota2.o

pilota2: pilota2.o winsuport2.o memoria.o semafor.o missatge.o
	$(CC) $(CFLAGS) pilota2.o winsuport2.o memoria.o semafor.o missatge.o -o pilota2 $(LIBS)

# --- FASE 3 ---

mur3.o: mur3.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c mur3.c -o mur3.o -lpthread

mur3: mur3.o winsuport2.o memoria.o semafor.o missatge.o
	$(CC) $(CFLAGS) mur3.o winsuport2.o memoria.o semafor.o missatge.o -o mur3 $(LIBS) -lpthread

pilota3.o: pilota3.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c pilota3.c -o pilota3.o

pilota3: pilota3.o winsuport2.o memoria.o semafor.o missatge.o
	$(CC) $(CFLAGS) pilota3.o winsuport2.o memoria.o semafor.o missatge.o -o pilota3 $(LIBS)

# --- Neteja ---
# Executar 'make clean' per esborrar els binaris i fitxers objecte
clean:
	rm -f *.o mur0 mur1 pilota1 mur2 pilota2 mur3 pilota3
