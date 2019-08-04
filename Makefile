###################################################################
#       This is the makefile that creates the `ramdisk` executable
#
#       RAMDISK is an in-memory non-persistent filesystem
#       and supports basic POSIX/Unix commands such as
#	read, write and append.
#	RAMDISK never sync data onto the disk.
#
#       -----------------------------------
#       Author: Ramya Vijayakumar
#       -----------------------------------
##################################################################

CC=gcc
CFLAGS= -g $(PKGFLAGS) -lm
PKGFLAGS=`pkg-config fuse --cflags --libs`
OBJ= ramdisk.o ramnode.o

all: ramdisk

%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@ 
        
ramdisk: $(OBJ)
	$(CC) $^ $(CFLAGS) -o $@ 


clean:
	rm *.o ramdisk
