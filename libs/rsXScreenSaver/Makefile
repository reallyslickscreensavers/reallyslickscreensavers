INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = rsXScreenSaver.o \
	../util/arguments.o


all: $(OBJECTS)
	ar cr ../../lib/rsXScreenSaver.a $(OBJECTS)

clean:
	rm $(OBJECTS)


.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
