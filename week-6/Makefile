
FLAGS = -Wall -pthread

all: t0 t1 backslash

clean:
	rm -f t0 t1 backslash

t0: t0.c
	gcc -o t0 t0.c $(FLAGS)

t1: t1.c
	gcc -o t1 t1.c $(FLAGS)

backslash: backslash.c
	gcc -o backslash backslash.c $(FLAGS)
