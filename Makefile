CC = gcc
LUA_PKG := $(shell if pkg-config --exists lua; then echo lua; \
					elif pkg-config --exists lua5.4; then echo lua5.4; \
					elif pkg-config --exists lua5.3; then echo lua5.3; \
					elif pkg-config --exists lua5.2; then echo lua5.2; \
					elif pkg-config --exists lua5.1; then echo lua5.1; fi)
CFLAGS = -std=c99 -pedantic -Wall -Wextra $(shell pkg-config --cflags $(LUA_PKG))
LDFLAGS = -lm $(shell pkg-config --libs $(LUA_PKG))

SRC = tpv.c imageinfo.c drw.c
OBJ = $(SRC:.c=.o)
TARGET = tpv

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.so: %.c
	$(CC) -shared -fPIC $(CFLAGS) $< -o $@

support: imageinfo.so drw.so

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean $(TARGET)

,PHONY: clean