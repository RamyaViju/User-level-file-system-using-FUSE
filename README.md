# User-level-file-system-using-FUSE
# Author
#
# Ramya Vijayakumar

# RAMDISK is an in-memory non-persistent filesystem
# and supports basic POSIX/Unix commands such as
# read, write and append.
# RAMDISK never sync data onto the disk.

# Invocation

	make clean
	make

	./ramdisk < mount point > < size in MB > [< file name >]

	`< mount point >` and `< size in MB >` are mandatory. `<file name>` is the file where the files and folders in the RAMdisk are dumped into and read when remounted.
