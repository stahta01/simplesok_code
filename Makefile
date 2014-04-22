
CFLAGS = -O3 -Wall -Wextra -std=gnu89 -pedantic -Wno-long-long
CLIBS = `sdl2-config --libs`

all: simplesok

simplesok: sok.o sok_core.o crc32.o save.o gzbmp.o
	gcc $(CFLAGS) sok.o sok_core.o crc32.o save.o gzbmp.o -o simplesok $(CLIBS)

sok.o: sok.c
	gcc -c $(CFLAGS) sok.c -o sok.o

sok_core.o: sok_core.c
	gcc -c $(CFLAGS) sok_core.c -o sok_core.o

crc32.o: crc32.c
	gcc -c $(CFLAGS) crc32.c -o crc32.o

save.o: save.c
	gcc -c $(CFLAGS) save.c -o save.o

gzbmp.o: gzbmp.c
	gcc -c $(CFLAGS) gzbmp.c -o gzbmp.o

clean:
	rm -f *.o simplesok file2c

data: data_img.h data_lev.h data_fnt.h data_skn.h data_ico.h

data_img.h: img/*.png file2c png2bmp
	echo "/* This file is part of the sok project. */" > data_img.h
	for x in img/*.png ; do ./png2bmp $$x ; done
	for x in img/*.bmp ; do gzip -n -9 $$x ; done
	for x in img/*.bmp.gz ; do ./file2c $$x >> data_img.h ; done
	rm img/*.bmp.gz

data_skn.h: skin/*.png file2c
	echo "/* This file is part of the sok project. */" > data_skn.h
	for x in skin/*.png ; do ./png2bmp $$x ; done
	for x in skin/*.bmp ; do gzip -n -9 $$x ; done
	for x in skin/*.bmp.gz ; do ./file2c $$x >> data_skn.h ; done
	rm skin/*.bmp.gz

data_lev.h: levels/*.xsb file2c
	echo "/* This file is part of the sok project. */" > data_lev.h
	for x in levels/*.xsb ; do ./file2c $$x >> data_lev.h ; done

data_fnt.h: font/*.png file2c
	echo "/* This file is part of the sok project. */" > data_fnt.h
	for x in font/*.png ; do ./png2bmp $$x ; done
	for x in font/*.bmp ; do gzip -n -9 $$x ; done
	for x in font/*.bmp.gz ; do ./file2c $$x >> data_fnt.h ; done
	rm font/*.bmp.gz

data_ico.h: simplesok.png
	echo "/* This file is part of the sok project. */" > data_ico.h
	./png2bmp simplesok.png
	gzip -n -9 simplesok.bmp
	./file2c simplesok.bmp.gz >> data_ico.h
	rm simplesok.bmp.gz

file2c: file2c.c
	gcc $(CFLAGS) file2c.c -o file2c

png2bmp: png2bmp.c
	gcc $(CFLAGS) png2bmp.c -o png2bmp `sdl2-config --libs` -lSDL2_image
