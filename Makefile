all: sps.c
	gcc -std=c99 -Wall -Werror -Wextra -g -O0 sps.c -o sps