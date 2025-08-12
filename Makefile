CC = gcc
CFLAGS = -Iinclude -I/opt/homebrew/include -Wall -Wextra -std=c11
LDFLAGS = -L/opt/homebrew/lib -lcurl -lsqlite3 -ljansson
SRC_DIR = src
INCLUDE_DIR = include
DIST_DIR = dist
EXECUTABLE = cbot

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(DIST_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean

all: $(DIST_DIR)/$(EXECUTABLE)

$(DIST_DIR)/$(EXECUTABLE): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(DIST_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(DIST_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(DIST_DIR)
