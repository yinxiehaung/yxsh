TARGET = yxsh
YXSH_SRC_DIR = src
YXSH_INCLUDE_DIR = include
CFLAG = -Wall -g -O3 -I$(YXSH_INCLUDE_DIR)
OBJ_DIR = object
CC = gcc

SRCS = $(wildcard $(YXSH_SRC_DIR)/*.c)
OBJS = $(patsubst $(YXSH_SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean
all: $(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(CFLAG) $^ main.c -o $@
$(OBJ_DIR)/%.o:$(YXSH_SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAG) -c $< -o $@
