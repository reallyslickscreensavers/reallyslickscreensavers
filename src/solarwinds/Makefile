INCLUDE = -I. -I../../include
LIBRARY = -lm -lGL -lGLU -lX11 -lstdc++ ../../lib/rsXScreenSaver.a ../../lib/rsMath.a ../../lib/rsText.a
CFLAGS = -O3 -DRS_XSCREENSAVER -DGL_GLEXT_PROTOTYPES $(INCLUDE)

OBJECTS = SolarWinds.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o ../../bin/solarwinds ${LIBRARY}

clean:
	rm ${OBJECTS}
	

.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
