OBJS := main_aio.o
BIN := aio_copy
CC := gcc
LDFLAGS := -lrt
CFLAGS := -m64 -Wall -g

all: $(BIN)

$(BIN): $(OBJS)
	gcc -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	gcc -o $@ -c $< $(CFLAGS) 

clean:
	rm *.o $(BIN)
