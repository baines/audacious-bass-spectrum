FLAGS := $(shell pkg-config --cflags gtk+-2.0 audacious) -fPIC
LIBS  := $(shell pkg-config --libs gtk+-2.0 audacious)

cairo_bass_mod.so: plugin.o spectrum.o
	g++ $(FLAGS) $^ -shared -o $@ -lfftw3 $(LIBS)

spectrum.o: spectrum.cpp spectrum.h
	g++ $(FLAGS) -c $< -o $@
	
plugin.o: plugin.cpp spectrum.h
	g++ -std=c++11 $(FLAGS) -c $< -o $@

clean:
	rm spectrum.o plugin.o cairo_bass_mod.so
