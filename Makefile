GC=gcc
CFLAGS=-Wall -Wextra -O2 -Iinclude

DAEMON_BIN=fanctld
DAEMON_SRCS=src/main.c src/raspberry.c
PI_SRCS=src/main.c src/raspberry_pi.c

GUI_BIN=gui
GUI_SRC=src/gui.c

.PHONY: all ui pi clean run

all: ${DAEMON_BIN}

${DAEMON_BIN}: ${DAEMON_SRCS}
	${GC} ${CFLAGS} -o ${DAEMON_BIN} ${DAEMON_SRCS}

pi: ${PI_SRCS}
	${GC} ${CFLAGS} -o ${DAEMON_BIN} ${PI_SRCS} -lgpiod

ui:
	${GC} ${CFLAGS} `pkg-config --cflags gtk+-3.0 pango` -o ${GUI_BIN} ${GUI_SRC} `pkg-config --libs gtk+-3.0 pango`

clean:
	- rm -f ${DAEMON_BIN} ${GUI_BIN} *.o src/*.o

run: ${DAEMON_BIN}
	./${DAEMON_BIN}
