CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread -lrt -lncurses
SRC = main.c ipc.c logger.c bidders.c
OBJ = $(SRC:.c=.o)
TARGET = auction

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c auction.h
	$(CC) $(CFLAGS) -c $<

# clean up object files and nuke stale shared memory
clean:
	rm -f *.o $(TARGET) history.log
	-ipcrm -M 0xABC1 2>/dev/null || true
	-rm -f /dev/shm/sem.auc_*
