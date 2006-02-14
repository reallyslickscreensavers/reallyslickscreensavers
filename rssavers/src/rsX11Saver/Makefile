INCLUDE = -I../../include
COMPILE = gcc $(INCLUDE) -O3
OBJECTS = rsX11Saver.o


all: $(OBJECTS)
	ar cr ../../lib/rsX11Saver.a $(OBJECTS)


.SUFFIXES: .cpp .o

.cpp.o:
	$(COMPILE) -o $@ -c $<
