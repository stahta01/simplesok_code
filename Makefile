
CFLAGS = -O3 -Wall -Wextra -std=gnu89 -pedantic
CLIBS = `sdl2-config --libs` -lSDL2_image

all: simplesok

simplesok: sok.o sok_core.o crc32.o save.o
	gcc $(CFLAGS) sok.o sok_core.o crc32.o save.o -o simplesok $(CLIBS)

sok.o: sok.c
	gcc -c $(CFLAGS) sok.c -o sok.o

sok_core.o: sok_core.c
	gcc -c $(CFLAGS) sok_core.c -o sok_core.o

crc32.o: crc32.c
	gcc -c $(CFLAGS) crc32.c -o crc32.o

save.o: save.c
	gcc -c $(CFLAGS) save.c -o save.o

clean:
	rm -f *.o simplesok file2c

data: data_img.h data_lev.h data_fnt.h data_skn.h data_ico.h

data_img.h: img/*.png file2c
	optipng -o7 img/*.png
	echo "/* This file is part of the sok project. */" > data_img.h
	for x in img/*.png ; do ./file2c $$x >> data_img.h ; done

data_skn.h: skin/*.png file2c
	optipng -o7 skin/*.png
	echo "/* This file is part of the sok project. */" > data_skn.h
	for x in skin/*.png ; do ./file2c $$x >> data_skn.h ; done

data_lev.h: levels/*.xsb file2c
	echo "/* This file is part of the sok project. */" > data_lev.h
	for x in levels/*.xsb ; do ./file2c $$x >> data_lev.h ; done

data_fnt.h: font/*.png file2c
	optipng -o7 font/*.png
	echo "/* This file is part of the sok project. */" > data_fnt.h
	for x in font/*.png ; do ./file2c $$x >> data_fnt.h ; done

data_ico.h: simplesok.png
	optipng -o7 simplesok.png
	echo "/* This file is part of the sok project. */" > data_ico.h
	./file2c simplesok.png >> data_ico.h

file2c: file2c.c
	gcc $(CFLAGS) file2c.c -o file2c
