INCLUDE = -I../../include
LIBRARY = -L/usr/X11R6/lib -L../../lib \
	-lm -lGL -lGLU -lglut -lXmu -lXi ../../lib/Implicit.a ../../lib/rsMath.a
CFLAGS = -O3 $(INCLUDE)

OBJECTS = ImplicitDemo.o



all: ${OBJECTS}
	gcc ${CFLAGS} ${OBJECTS} -o ../../bin/ImplicitDemo ${LIBRARY}



.SUFFIXES: .cpp .o

.cpp.o:
	gcc ${CFLAGS} -o $@ -c $<
