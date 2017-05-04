INCLUDE = -I. -I../../include
LIBRARY = -lm -lGL -lGLU -lX11 -lstdc++ ../../lib/rsXScreenSaver.a ../../lib/rsMath.a ../../lib/Implicit.a ../../lib/rsText.a  ../../lib/Rgbhsl.a
CFLAGS = -O3 -DRS_XSCREENSAVER -DGL_GLEXT_PROTOTYPES $(INCLUDE)

OBJECTS = Hyperspace.o \
	causticTextures.o \
	flare.o \
	goo.o \
	splinePath.o \
	starBurst.o \
	stretchedParticle.o \
	tunnel.o \
	wavyNormalCubeMaps.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o ../../bin/hyperspace ${LIBRARY}

clean:
	rm ${OBJECTS}
	

.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
