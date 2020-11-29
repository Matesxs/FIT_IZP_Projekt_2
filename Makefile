all: sps.c
	gcc -std=c99 -Wall -Werror -Wextra -g -O1 sps.c -o sps