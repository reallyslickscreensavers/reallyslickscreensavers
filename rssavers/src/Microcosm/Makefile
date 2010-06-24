INCLUDE = -I. -I../../include
LIBRARY = -lm -lGL -lGLU ../../lib/rsXScreenSaver.a ../../lib/rsMath.a ../../lib/Implicit.a ../../lib/Rgbhsl.a ../../lib/rsText.a -pthread
CFLAGS = -O3 -DRS_XSCREENSAVER -DGL_GLEXT_PROTOTYPES $(INCLUDE) -msse -mfpmath=sse

OBJECTS = Microcosm.o \
	MirrorBox.o \
	rsCamera.o \
	Gizmo.o \
	Texture1D.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o ../../bin/microcosm ${LIBRARY}

clean:
	rm ${OBJECTS}
	

.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<

.c.o:
	gcc ${CFLAGS} -o $@ -c $<
	
