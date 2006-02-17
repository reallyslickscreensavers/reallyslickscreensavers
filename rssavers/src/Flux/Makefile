INCLUDE = -I../../include
LIBRARY = -L../../lib -lm -lGL -lGLU ../../lib/rsX11Saver.a ../../lib/Rgbhsl.a ../../lib/rsText.a
CFLAGS = -O3 -DFOR_XSCREENSAVER $(INCLUDE)

OBJECTS = Flux.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o flux ${LIBRARY}



.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
