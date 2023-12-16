GC=gcc
FILE=src/main.c
GUI_FILE=src/gui.c
INCLUDE_DIR=/include
OUTPUT=app.o

all:
	${GC} ${FILE} -I/${INCLUDE_DIR} -o ${OUTPUT}

ui:
	${GC}  `pkg-config --cflags gtk+-3.0 pango`  -o gui.o ${GUI_FILE} `pkg-config --libs gtk+-3.0 pango`

clean:
	- rm *.o 2> /dev/null &
	- rm src/*.o 2> /dev/null

run:
	./${OUTPUT}
