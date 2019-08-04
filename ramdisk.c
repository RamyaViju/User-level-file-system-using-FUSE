/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

#include "ramnode.h"

ramNode *head;
long int ramFSSize = 0;
long int ramAvailableSize = 0;
char *persistentFile;
char *dupPersistentFile;

int isPersistentEnabled= 0;

void initNodes() {

	ramNode *n = (ramNode *) malloc(sizeof(ramNode));
	struct fuse_context *f = fuse_get_context();
	strcpy(n->name, "/");

	n->gid = f->gid;
	n->uid = f->uid;

	n->type = DIR_TYPE;
	n->mode = 0777;

	n->atime = time(NULL);
	n->ctime = time(NULL);
	n->mtime = time(NULL);
	n->data = NULL;
	n->next = NULL;

	head = n;

	if(access( persistentFile, F_OK ) != -1 ) {
		// load the data from here
		FILE *fp = fopen(persistentFile, "r");
		char *name = (char*)malloc(256);
		int type;
		mode_t mode;
		gid_t gid;
		uid_t uid;
		size_t size;
		char *data;
		time_t atime;
		time_t mtime;
		time_t ctime;
		while(!feof(fp)) {

			fscanf(fp, "%s", name);
			if(searchNode(head, name) != NULL)
				break;
			ramNode *t = (ramNode *)malloc(sizeof(ramNode));

			// Name of the file

			strcpy(t->name, name);

			// type
			char temp[10];
			fscanf(fp, "%s", temp);
			type = atoi(temp);
			t->type = type;


			// mode
			fscanf(fp, "%s", temp);
			mode = atoi(temp);
			t->mode = mode;

			// gid
			fscanf(fp, "%s", temp);
			gid = atoi(temp);
			t->gid = gid;

			//uid
			fscanf(fp, "%s", temp);
			uid = atoi(temp);
			t->uid = uid;

			//size
			fscanf(fp, "%s", temp);
			size = atoi(temp);
			t->size = size;


			if(type == DIR_TYPE) {
				// No need to read data
				t->data = NULL;

				// a time
				fscanf(fp, "%s", temp);
				atime = atol(temp);
				t->atime = atime;

				// m time
				fscanf(fp, "%s", temp);
				mtime = atol(temp);
				t->mtime = mtime;

				// c time
				fscanf(fp, "%s", temp);
				ctime = atol(temp);
				t->ctime = ctime;


			} else if(type == FILE_TYPE) {
				// No need to read data
				t->data = (char *) malloc(t->size);
				fseek(fp, 1, SEEK_CUR);
				fread(t->data, 1, t->size, fp);

				// a time
				fscanf(fp, "%s", temp);
				atime = atol(temp);
				t->atime = atime;

				// m time
				fscanf(fp, "%s", temp);
				mtime = atol(temp);
				t->mtime = mtime;

				// c time
				fscanf(fp, "%s", temp);
				ctime = atol(temp);
				t->ctime = ctime;
			}


			addNode(head, t);
		}

		fclose(fp);

	}
}


static int ramdisk_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	ramNode *temp = searchNode(head, path);

	if(temp == NULL)
		return -ENOENT;

	stbuf->st_ctime = temp->ctime;
	stbuf->st_atime = temp->atime;
	stbuf->st_mtime = temp->mtime;
	stbuf->st_gid = temp->gid;
	stbuf->st_uid = temp->uid;

	if (temp != NULL && temp->type == DIR_TYPE) {
		stbuf->st_mode = S_IFDIR | temp->mode;
		stbuf->st_nlink = 2;
	} else if (temp != NULL && temp->type == FILE_TYPE) {
		stbuf->st_mode = S_IFREG | temp->mode;
		stbuf->st_nlink = 1;
		stbuf->st_size = temp->size;

	}

	return res;
}

static int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	char *tempName;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	ramNode *temp = head;
	while (temp != NULL) {
		tempName = (char *) malloc(strlen(temp->name));
		strcpy(tempName, temp->name);

		char *parentName = dirname(tempName);
		// Check whether the node and the path are same. It SHOULDN'T be
		if(strcmp(temp->name, path) != 0) {
			// Now check whether the parent of the file and the path are same.
			if(strcmp(parentName, path) == 0) {
				strcpy(tempName, temp->name);
				filler(buf, basename(tempName), NULL, 0);
			}
		}

		free(tempName);

		temp = temp->next;
	}

	return 0;
}

static int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	ramNode *temp = searchNode(head, path);
	if (temp == NULL)
		return -ENOENT;

	return 0;
}

static int ramdisk_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	ramNode *temp = searchNode(head, path);

	if (temp == NULL)
		return -ENOENT;
	if(temp->type == DIR_TYPE)
		return -ENOENT;

	temp->atime = time(NULL);
	len = temp->size;
	if(len == 0) {
		return 0;
	}


	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, temp->data + offset, size);
	} else
		size = 0;

	return size;
}

static int ramdisk_mkdir(const char *path, mode_t mode) {
	int res = 0;

	if(ramAvailableSize < sizeof(ramNode)) {
		return -ENOSPC;
	}
	ramNode *n = (ramNode *) malloc(sizeof(ramNode));
	ramAvailableSize = ramAvailableSize - sizeof(ramNode);

	struct fuse_context *f = fuse_get_context();

	strcpy(n->name, path);
	n->gid = f->gid;
	n->uid = f->uid;

	n->mode = mode;
	n->type = DIR_TYPE;
	n->atime = time(NULL);
	n->ctime = time(NULL);
	n->mtime = time(NULL);
	n->next = NULL;
	n->size = 0;
	n->data = NULL;

	addNode(head, n);

	return res;
}

static int ramdisk_rmdir(const char *path) {
	int res;

	res = deleteNode(head, path);

	if(res == 0) {
		ramAvailableSize = ramAvailableSize + sizeof(ramNode);
	}

	return res;
}

static int ramdisk_create(const char * path, mode_t mode, struct fuse_file_info *fi) {
	int res = 0;

	if(ramAvailableSize < sizeof(ramNode)) {
		return -ENOSPC;
	}

	ramNode *n = (ramNode *) malloc(sizeof(ramNode));
	struct fuse_context *f = fuse_get_context();


	strcpy(n->name, path);
	n->gid = f->gid;
	n->uid = f->uid;

	n->mode = mode;
	n->type = FILE_TYPE;
	n->atime = time(NULL);
	n->ctime = time(NULL);
	n->mtime = time(NULL);

	n->next = NULL;
	n->size = 0;
	n->data = (char *)malloc(sizeof(char));
	n->data[0] = '\0';

	addNode(head, n);
	ramAvailableSize = ramAvailableSize - sizeof(ramNode);

	return res;
}

static int ramdisk_rename(const char *source, const char *dest) {

	ramNode *temp = searchNode(head, source);

	if(temp == NULL)
		return -ENOENT;

	strcpy(temp->name, dest);

	temp->mtime = time(NULL);

	return 0;
}

static int ramdisk_utimens(const char* path, const struct timespec tv[2]) {
	ramNode *temp = searchNode(head, path);

	if(temp == NULL)
		return -ENOENT;

	temp->atime = tv[0].tv_sec;
	temp->mtime = tv[1].tv_sec;

	return 0;
}

static int ramdisk_write(const char *path, const char *buf, size_t size,
	     off_t offset, struct fuse_file_info *fi){

	ramNode *temp = searchNode(head, path);


	if (temp == NULL)
			return -ENOENT;


	if(size != 0) {
		if(ramAvailableSize < size) {
			return -ENOSPC;
		}

		temp->data = (char*) realloc(temp->data, offset + size);
		memcpy(temp->data+offset, buf, size);

		temp->size = offset + size;
		ramAvailableSize = ramAvailableSize - size;

	}

	return size;
}
static int ramdisk_unlink(const char* path) {
	int res = 0;
	ramNode *temp = searchNode(head, path);
	if(temp != NULL) {
		ramAvailableSize = ramAvailableSize + sizeof(ramNode) + temp->size;
	}
	res = deleteFile(head, path);

	return res;
}

static int ramdisk_chmod(const char* path, mode_t mode) {
	ramNode *n = searchNode(head, path);

	if(n == NULL)
		return -ENOENT;

	n->mode = mode;

	return 0;
}

static int ramdisk_chown(const char* path, uid_t uid, gid_t gid) {
	ramNode *n = searchNode(head, path);

	if(n == NULL)
		return -ENOENT;

	n->gid = gid;
	n->uid = uid;
	return 0;
}











static int ramdisk_truncate(const char* path, off_t size) {
	ramNode * temp = searchNode(head, path);

	if(temp == NULL)
		return -ENOENT;

	if(size == 0) {
		ramAvailableSize = ramAvailableSize + temp->size;
		temp->data = realloc(temp->data, size+1);
		temp->data[0] = '\0';
	} else {
		ramAvailableSize = ramAvailableSize + temp->size - size;
		temp->data = realloc(temp->data, size+1);
		temp->data[size] = '\0';
	}


	return 0;
}


static int ramdisk_opendir(const char *path, struct fuse_file_info * fi) {
	ramNode *n = searchNode(head, path);

	if(n == NULL)
		return -ENOENT;

	if(n->type != DIR_TYPE)
		return -ENOTDIR;

	return 0;
}


static void ramdisk_destroy(void* private_data) {
	if(isPersistentEnabled == 1) {
		FILE *fp = fopen(dupPersistentFile, "w");

		ramNode *temp = head->next;
		while(temp != NULL) {

			// Write the file name
			fprintf(fp, "%s\n", temp->name);

			// type
			fprintf(fp, "%d\n", temp->type);
			// write(fd, "\n", 1);

			// mode
			fprintf(fp, "%d\n", temp->mode);

			// gid
			fprintf(fp, "%d\n", temp->gid);

			// uid
			fprintf(fp, "%d\n", temp->uid);

			// size
			fprintf(fp, "%d\n", temp->size);

			// data
			if(temp->type == FILE_TYPE) {
				fwrite(temp->data, sizeof(char), temp->size, fp);
			}


			// atime
			fprintf(fp, "%ld\n", temp->atime);

			// mtime
			fprintf(fp, "%ld\n", temp->mtime);


			// ctime
			fprintf(fp, "%ld\n", temp->ctime);




			temp = temp->next;
		}

		fclose(fp);
	}
}




static struct fuse_operations hello_oper = {
	.getattr	= ramdisk_getattr,
	.readdir	= ramdisk_readdir,
	.open		= ramdisk_open,
	.opendir 	= ramdisk_opendir,
	.read		= ramdisk_read,
	.mkdir 		= ramdisk_mkdir,
	.rmdir		= ramdisk_rmdir,
	.create 	= ramdisk_create,
	.rename 	= ramdisk_rename,
	.utimens 	= ramdisk_utimens,
	.write		= ramdisk_write,

	//.access = ramdisk_access,
	.unlink = ramdisk_unlink,
	.chmod = ramdisk_chmod,
	.chown = ramdisk_chown,

	// .flush 		= ramdisk_flush,

	.truncate = ramdisk_truncate,
	// .releasedir = ramdisk_releasedir,

	.destroy = ramdisk_destroy,



};


int main(int argc, char *argv[])
{

    char *newArgs[argc-1];	//Change this to argc-1
    int newArgc = 2;
	if(argc < 3) {
		fprintf(stderr, "Run the program: ./ramdisk <mountpoint> <size in MB> {file Name}\n");
		return 1;
	} else if(argc == 3) {
		newArgs[0] = strdup(argv[0]);
		newArgs[1] = strdup(argv[1]);

		ramFSSize = atol(argv[2]);
		ramFSSize = ramFSSize * 1024 * 1024;

	} else if(argc ==4) {
		newArgs[0] = strdup(argv[0]);
		newArgs[1] = strdup(argv[1]);
		ramFSSize = atol(argv[2]);

		ramFSSize = ramFSSize * 1024 * 1024;
		persistentFile = strdup(argv[3]);
		isPersistentEnabled = 1;

		dupPersistentFile = (char *)malloc(256);
		strcpy(dupPersistentFile, get_current_dir_name());
		strcat(dupPersistentFile, "/");
		strcat(dupPersistentFile, persistentFile);
	}
	ramAvailableSize = ramFSSize;

	initNodes();
	return fuse_main(newArgc, newArgs, &hello_oper, NULL);
}
