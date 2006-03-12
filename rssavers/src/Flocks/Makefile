INCLUDE = -I../../include
LIBRARY = -lm -lGL -lGLU ../../lib/rsXScreenSaver.a ../../lib/Rgbhsl.a ../../lib/rsText.a
CFLAGS = -O3 -DRS_XSCREENSAVER $(INCLUDE)

OBJECTS = Flocks.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o ../../bin/flocks ${LIBRARY}

clean:
	rm ${OBJECTS}
	

.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
