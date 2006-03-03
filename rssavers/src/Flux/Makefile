INCLUDE = -I../../include
LIBRARY = -L../../lib -lm -lGL -lGLU ../../lib/rsXScreenSaver.a ../../lib/Rgbhsl.a ../../lib/rsText.a
CFLAGS = -O3 -DRS_XSCREENSAVER $(INCLUDE)

OBJECTS = Flux.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o flux ${LIBRARY}

clean:
	rm ${OBJECTS}
	

.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
