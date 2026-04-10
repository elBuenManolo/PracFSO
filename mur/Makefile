# Makefile per a la Pràctica 2 d'FSO
# Compilació dels programes mur0, mur1 i pilota1

CC = gcc
CFLAGS = -Wall -g
LIBS = -lcurses

# Objectius principals (es compilen tots si fem 'make' a seques)
all: mur0 mur1 pilota1

# Rutines compartides (llibreria gràfica i IPC)
winsuport2.o: winsuport2.c winsuport2.h
	$(CC) $(CFLAGS) -c winsuport2.c -o winsuport2.o

memoria.o: memoria.c memoria.h
	$(CC) $(CFLAGS) -c memoria.c -o memoria.o

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

# --- Neteja ---
# Executar 'make clean' per esborrar els binaris i fitxers objecte
clean:
	rm -f *.o mur0 mur1 pilota1
