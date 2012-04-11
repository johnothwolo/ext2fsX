VERSION = 0.81
OBJ_UNDEL = find_del.o log.o
OBJ_FILE = apprentice.o ascmagic.o is_tar.o softmagic.o
H_FILE = e2undel.h file.h names.h tar.h magic.h common.h
C_FILE = libundel.c e2undel.c find_del.c log.c\
	 file.c apprentice.c ascmagic.c is_tar.c softmagic.c\
         compactlog.c
DOC_FILE = HISTORY BUGS README README.de INSTALL INSTALL.de
OTHER_FILE = Makefile
DEVEL_FILE = magic.h.ALT choose_inodes.c x.c

CC		= gcc
LIBS_UNDEL	= -lext2fs -lcom_err -lss
CFLAGS		= -O2 -g -Wall
LDFLAGS		= -Wall -g


e2undel-file: e2undel-file.o $(OBJ_UNDEL) $(OBJ_FILE)
	$(CC) $(LDFLAGS) -o e2undel e2undel-file.o\
	$(OBJ_UNDEL) $(OBJ_FILE) $(LIBS_UNDEL)
e2undel-file.o: e2undel.c e2undel.h
	gcc $(CFLAGS) -DWITH_FILE -c -o e2undel-file.o e2undel.c

e2undel: e2undel.o $(OBJ_UNDEL)
	$(CC) $(LDFLAGS) -o e2undel e2undel.o $(OBJ_UNDEL) $(LIBS_UNDEL)
e2undel.o: e2undel.c e2undel.h
	gcc $(CFLAGS) -c -o e2undel.o e2undel.c
$(OBJ_UNDEL) : e2undel.h


libundel: libundel.o
	ld -shared -soname libundel.so.1 -ldl -o libundel.so.1.0 libundel.o
libundel.o: libundel.c
	gcc -Wall -fPIC -c libundel.c

compactlog: compactlog.o log.o
	gcc $(CFLAGS) compactlog.o log.o -o compactlog

file:	file.o $(OBJ_FILE)
	$(CC) $(LDFLAGS) -o file file.o $(OBJ_FILE)
$(OBJ_FILE) : file.h names.h tar.h magic.h common.h

.c.o:
	gcc $(CFLAGS) -c $<

clean:
	rm -f $(OBJ_UNDEL) $(OBJ_FILE) file.o e2undel.o e2undel-file.o\
	libundel.so.1.0 compactlog.o compactlog e2undel
	

all:	libundel e2undel-file compactlog

dist:	e2undel.tgz
e2undel.tgz:
	sed -e 's/#define VERSION ".*"/#define VERSION "$(VERSION)"/'\
	e2undel.h > tmp
	if test -f tmp; then mv tmp e2undel.h; fi
	mkdir e2undel-$(VERSION)
	cp $(DOC_FILE) $(OTHER_FILE) $(H_FILE) $(C_FILE) e2undel-$(VERSION)/
	tar czf e2undel-$(VERSION).tgz e2undel-$(VERSION)/*
	rm -rf e2undel-$(VERSION)/

dist-devel: e2undel-devel.tgz
e2undel-devel.tgz:
	sed -e 's/#define VERSION ".*"/#define VERSION "$(VERSION)"/'\
	e2undel.h > tmp
	if test -f tmp; then mv tmp e2undel.h; fi
	mkdir e2undel-$(VERSION)
	cp $(DEVEL_FILE) $(DOC_FILE) $(OTHER_FILE) $(H_FILE) $(C_FILE) e2undel-$(VERSION)/
	tar czf e2undel-$(VERSION)-devel.tgz e2undel-$(VERSION)/*
	rm -rf e2undel-$(VERSION)/
