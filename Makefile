CC = gcc
CFLAGS = -std=c99 -O2 -pedantic -Wall -Wextra
CFLAGS_DEBUG = -std=c99 -O0 -g -fno-omit-frame-pointer
LDFLAGS = -lm

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

SRC = tpv.c
OBJ = $(SRC:.c=.o)
TARGET = tpv

ifdef nsatinc
	CFLAGS += -DNO_SATURATION_INCREASE
endif
ifdef truebw
	CFLAGS += -DTRUE_BW
endif
ifdef inline
	CFLAGS += -DDEFAULT_INLINE
endif
ifdef wbsize
	CFLAGS += -DWB_SIZE=$(WB_SIZE)
endif

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f tpv ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/tpv
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f tpv.1 ${DESTDIR}${MANPREFIX}/man1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/tpv.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/tpv
	rm -f ${DESTDIR}${MANPREFIX}/man1/tpv.1

debug:
	$(CC) $(SRC) -o $(TARGET).debug $(CFLAGS_DEBUG) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean $(TARGET)

.PHONY: clean install uninstall debug rebuild