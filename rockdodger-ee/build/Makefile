LIBRARIES=`sdl-config --libs` -lSDL_mixer -lSDL_image -lm
SOURCES=../src/rockdodger-ee.c ../src/SFont.c
DATA=../src/data.h
EXENAME=rockdodger-ee

all:	$(EXENAME)

$(EXENAME):
	./pack_data.sh
	gcc -Wall -W -O2 `sdl-config --cflags` -o ../$(EXENAME) $(SOURCES) $(LIBRARIES)
	rm $(DATA)

debug:
	./pack_data.sh
	gcc -Wall -W -g `sdl-config --cflags` -o ../$(EXENAME) $(SOURCES) $(LIBRARIES)
	rm $(DATA)

clean:
	rm ../$(EXENAME)

