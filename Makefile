CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -pthread
SRCS    = main.c producer_consumer.c thread_pool.c async.c
OBJS    = $(SRCS:.c=.o)
TARGET  = vendas

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run