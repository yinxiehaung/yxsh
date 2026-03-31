TARGET = yxsh
YXSH_SRC_DIR = src
YXSH_INCLUDE_DIR = include
CFLAG = -Wall -g -O3 -I$(YXSH_INCLUDE_DIR)
OBJ_DIR = bin
CC = gcc

SRCS = $(wildcard $(YXSH_SRC_DIR)/*.c)
OBJS = $(patsubst $(YXSH_SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean
all: $(TARGET)

clean:
	rm $(OBJ_DIR)/*.o $(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(CFLAG) $^ main.c -o $@
$(OBJ_DIR)/%.o:$(YXSH_SRC_DIR)/%.c
	$(CC) $(CFLAG) -c $< -o $@
