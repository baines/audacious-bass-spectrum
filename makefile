LIBS:=$(shell pkg-config --cflags --libs gtk+-3.0) $(shell pkg-config --cflags --libs audacious)

cairo_bass_mod.so: plugin.o spectrum.o
	g++ -fPIC $^ -shared -o $@ -lfftw3

spectrum.o: spectrum.cpp
	g++ -fPIC $(LIBS) -c $< -o $@
	
plugin.o: plugin.c
	gcc -fPIC $(LIBS) -c $< -o $@
	
clean:
	rm spectrum.o plugin.o cairo_bass_mod.so
