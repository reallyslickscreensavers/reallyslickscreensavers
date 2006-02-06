INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = rsVec.o \
	rsQuat.o \
	rsMatrix.o


all: $(OBJECTS)
	ar cr ../../lib/rsMath.a $(OBJECTS)


.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
