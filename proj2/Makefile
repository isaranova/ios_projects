#   Author: Ivana Saranova (xsaran02)
#   Date: 30.4.2018  
#   Makefile for IOS project 2
#
#   Inspired by Makefile from Tomas Brückner

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lpthread
BIN=proj2
SOURCE=proj2.c

all:
	$(CC) $(CFLAGS) $(SOURCE) -o $(BIN) $(LFLAGS)

clean:
	rm $(BIN)
