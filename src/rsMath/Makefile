INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = rsVec.o \
	rsVec4.o \
	rsQuat.o \
	rsMatrix.o


all: $(OBJECTS)
	ar cr ../../lib/rsMath.a $(OBJECTS)

clean:
	rm ${OBJECTS}

.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
