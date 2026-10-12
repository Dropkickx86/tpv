CC = gcc
CFLAGS = -std=c99 -O2 -pedantic -Wall -Wextra
CFLAGS_DEBUG = -std=c99 -O0 -g -fno-omit-frame-pointer
LDFLAGS = -lm

SRC = tpv.c
OBJ = $(SRC:.c=.o)
TARGET = tpv

ifdef nsatinc
	CFLAGS += -DNO_SATURATION_INCREASE
endif
ifdef truebw
	CFLAGS += -DTRUE_BW
endif

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(CC) $(SRC) -o $(TARGET).debug $(CFLAGS_DEBUG) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean $(TARGET)

,PHONY: clean