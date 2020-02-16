.PHONY = all libs test clean final

SHELL = /bin/bash

SRC = ./src
HEADS = ./src/headers

TARGETOUT = client.out server.out
TARGETLIBS = $(SRC)/libosclient.a $(SRC)/libserverfuns.a

SERVER = $(SRC)/server.c $(SRC)/worker.c $(HEADS)/serverfuns.h $(HEADS)/utils.h libserverfuns.a
CLIENT = $(SRC)/client.c $(HEADS)/osclient.h libosclient.a
SKTCOMM = $(HEADS)/sktcomm.h

CFLAGS = -std=c99 -Wall -o

all : $(TARGETOUT)

final :
	tar -zcvf alessandro_puccia.tar.gz $(SRC) *.out makefile testsum.sh relazione.pdf

clean :
	rm -f *.{a,o}

libs : $(TARGETLIBS)

test :
	./server.out & \
	echo -e "*****Inizio del test - $$(date)*****\n" > testout.log; \
	echo -e "---Fase 1---\n" >> testout.log;
	for ((i=1; i<=50; i++)); do \
		./client.out client$$i 1 >> testout.log & pid="$$pid $$!" ; \
	done; \
	wait; \
	echo -e "\n---Fase 2---\n" >> testout.log;
	for ((i=1; i<=30; i++)); do \
		./client.out client$$i 2 >> testout.log & pid="$$pid $$!"; \
	done; \
	for ((i=31; i<=50; i++)); do \
		./client.out client$$i 3 >> testout.log & pid="$$pid $$!"; \
	done; \
	wait; \
	killall -USR1 server.out; \
	echo -e "*****Test completato*****\n\n" >> testout.log; \
	killall -QUIT server.out; \

client.out : $(CLIENT) $(SKTCOMM)
	$(CC) $< -g $(CFLAGS) $@ -L . -losclient

server.out : $(SERVER) $(SKTCOMM)
	$(CC) $< $(SRC)/worker.c -g $(CFLAGS) $@ -L . -lserverfuns -pthread

lib%.a : %.o sktcomm.o
	$(AR) $(ARFLAGS) $@ $^

%.o : $(SRC)/%.c $(HEADS)/%.h
	$(CC) $< -c $(CFLAGS) $@

osclient.o : $(SRC)/osclient.c $(HEADS)/osclient.h $(HEADS)/sktcomm.h
	$(CC) $< -c $(CFLAGS) $@
