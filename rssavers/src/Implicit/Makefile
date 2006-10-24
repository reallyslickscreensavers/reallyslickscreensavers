INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = impCubeTables.o \
	impCubeVolume.o \
	impSurface.o \
	impShape.o \
	impSphere.o \
	impEllipsoid.o \
	impTorus.o \
	impHexahedron.o \
	impKnot.o


all: $(OBJECTS)
	ar cr ../../lib/Implicit.a $(OBJECTS)

clean:
	rm ${OBJECTS}


.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
