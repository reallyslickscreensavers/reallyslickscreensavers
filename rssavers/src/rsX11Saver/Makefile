INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = rsX11Saver.o \
	../util/arguments.o


all: $(OBJECTS)
	ar cr ../../lib/rsX11Saver.a $(OBJECTS)

clean:
	rm $(OBJECTS)


.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
