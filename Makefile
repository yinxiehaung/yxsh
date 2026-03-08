target = my_shell
CC = gcc
SRC = src/my_arena.c src/my_string.c src/shell_parser.c src/shell_executor.c 
CFLAG = -Wall -g -flto -o

my_shell:$(SRC)
	$(CC) $(CFLAG) $@ $^ main.c 
clean:
	rm -f *.o $(target)
