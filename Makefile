CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Wextra $(shell pkg-config --cflags lua5.4)
LDFLAGS = -lm $(shell pkg-config --libs lua5.4)

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