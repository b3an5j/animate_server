CC = gcc

CFLAGS  = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g -pthread
LDFLAGS = -pthread

INC_FLAGS = -Iclient/include -Iserver/include -Imisc/include -Ilibanimate/include
LIB_LINK  = -Llibanimate/lib -lanimate

OBJDIR = obj

## Sources
MISC_SRC = misc/src/fifo.c misc/src/errors.c misc/src/dbg.c

CLIENT_SRC = client/src/animate_client.c \
             client/src/c_helper.c\
             client/src/receiver.c\
             client/src/sender.c

SERVER_SRC = server/src/animate_server.c \
             server/src/s_helper.c \
             server/src/client_registry.c \
             server/src/task.c \
             server/src/threadpool.c \
             server/src/auth.c \
             server/src/commands.c

# Map .c to .o
MISC_OBJ   = $(patsubst %.c,$(OBJDIR)/%.o,$(MISC_SRC))
CLIENT_OBJ = $(patsubst %.c,$(OBJDIR)/%.o,$(CLIENT_SRC))
SERVER_OBJ = $(patsubst %.c,$(OBJDIR)/%.o,$(SERVER_SRC))

CLIENT_BIN = animate_client
SERVER_BIN = animate_server

.PHONY: default clean run-server run-client libanimate

default: $(SERVER_BIN) $(CLIENT_BIN)

libanimate:
	@test -d libanimate || \
	( echo "ERROR: libanimate not found" >&2; false )

## Linking

$(SERVER_BIN): $(SERVER_OBJ) $(MISC_OBJ) | libanimate
	$(CC) -o $@ $^ $(LIB_LINK) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJ) $(MISC_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

## Compile

# obj/<path>.o from <path>.c
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

## Run

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

## Clean

clean:
	rm -rf $(OBJDIR)
	rm -f $(SERVER_BIN) $(CLIENT_BIN)