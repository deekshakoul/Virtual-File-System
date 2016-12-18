/*
OS PROJECT:

Version_1
A Virtual File System that uses
FAT and sequential directory Implementation
with functions for file creation,
file deletion, file read/write.

Mahima Achhpal - 201301199
Deeksha Koul - 201301435

*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include "vfs.c"
#define MAX_F_NAME 32 //max_char that file name can have.
#define DATA_BLOCKS 2560 //acquire half of total space
#define MAX_FAT_SIZE 3//(no. of data blocks *size of int) /BLOCK_SIZE = 2.5 blocks

struct super_block
{
	 int version;
	  /* FAT information where it starts on disk*/
	 int fat_idx;
	 int fat_len;
	 /* directory information */
	 int dir_idx;
	 int dir_len;
	 /* data information */
	 int data_idx;

	 //int num_used_b;
	 int num_used_b;

};

struct dir_entry //it stores the information of a directory
{
 int used; //whether its free or not
 char name[MAX_F_NAME + 1]; //file name
 int size;//file size
 int head; //first data block of this file
 //cannot delete this file
};

struct file_descriptor
{// for now multiple fds not allowed
 int used; // file descriptor is in use
 int file; // the first block of current file who uses this file descriptor
 int offset;
};

struct super_block sb;
int *FAT; // a queue, Points to the head of the FAT table.
struct dir_entry *DIR ; // a queue of directories
int entries = 0; // no of entries in directory

int get_free_block();  //get the next free block required for allocation of file
int dir_create_file(char * name);
struct dir_entry* dir_list_file(char * name); /*searches for the file/directory given the name*/
int fs_create(char *name);  //files or directry is created
struct file_descriptor* fs_open(char *name);
int fs_delete(char *name);
void print_dir();
void print_FAT();
void init_super_block();
void make_fs(char *);
void load_Directory(); //Load directory from memory into directory structure.
void load_FAT();
void mount_fs(char *); // mounts a file system that is stored on a virtual disk with some name.
void write_back_Directory(); // similar to load_FAT
void write_back_FAT();
void unmount_fs(char *);
int fs_write(char * name, struct file_descriptor* fd, void *src, int nbytes); //writing nbytes to files/directory
int fs_read(char * name, struct file_descriptor* fd, void *dest, int nbytes); //reading nbytes from files/directory


int fs_write(char * name, struct file_descriptor* fd, void *src, int nbytes)
{// assume writes from starts, assumes nbytes multiple of block size
	int new_blocks, curr, free, i, blocks=0;
	struct dir_entry * dir;

	char buf[BLOCK_SIZE];

	if(nbytes == 0) //error handling.
		return 0;

	dir = dir_list_file(name); //search for the file and return the block number of the starting block of the file.

	new_blocks = (nbytes/ BLOCK_SIZE + 1) - (dir->size);  //new blocks taht are to be written
 //if there are not many free blocks available.
	if(new_blocks> (DATA_BLOCKS-sb.num_used_b)){
			printf("not enough blocks to write");
			return -1;
	}

	curr=(fd->file);
	//block_write

	memcpy(buf,(void *)(src + blocks*BLOCK_SIZE),BLOCK_SIZE);
	block_write(curr, buf);
	blocks++;// keeps track of blocks written

	if(new_blocks>0){
		/// curr points to last block;
		 while(FAT[curr]!=-1){    //inside the loop till EOF not reached
			 curr= FAT[curr];    //make links between the blocks of the same file.
		 }

		for(i=0; i<new_blocks; i++) {
		// need to allocate new blocks;

			free = get_free_block();
			printf("block %d allocated\n", free);
			FAT[curr] = free;
			curr = free;
			FAT[curr] = -1; //EOF

			memcpy(buf,(void *)(src + blocks*BLOCK_SIZE),BLOCK_SIZE); /* buf is destination where data of blocks are getting stored upto the BLOCK_SIZE  */
			block_write(curr, buf); //writing the data to the block
			blocks++;
			sb.num_used_b++;   //number of used blacks will be incresed

		}

		dir->size += new_blocks;
	}
	return (new_blocks+1);
}

int fs_read(char * name, struct file_descriptor* fd, void *dest, int nbytes)
{// assume reads from starts, assumes nbytes multiple of block size
	int blocksr, curr, i, blocks=0;
	struct dir_entry * dir;
	char buf[BLOCK_SIZE];

	if(nbytes == 0)
		return 0;

	dir = dir_list_file(name);

	blocksr = (nbytes/ BLOCK_SIZE ); // blocks to be read

	if(blocksr > (dir->size)){
		printf("not enough block to be read");
		return -1;
	}

	// reads from file: start block
	for(i=0,curr = fd->file; i<blocksr; i++){

		block_read(curr, buf);
		memcpy((void *)(dest + blocks*BLOCK_SIZE), buf,BLOCK_SIZE);
		blocks ++;

		curr = FAT[curr]; //pointer to the next block.

	}

	return blocksr; //blocks that have been read
}


int main(){

	char* name = "MyDisk.txt";
	int i;

	make_fs(name);
	init_super_block();
	mount_fs(name);

	printf("creating a file named mahima\n");
	struct file_descriptor * fd;
	char *file = "mahima";
	i = fs_create(file);
	fd = fs_open(file);

	print_dir();
    print_FAT();


	printf("creating a file named hello\n");

	fs_create("hello");

	print_FAT();
	print_dir();

	printf("creating a file named deeksha\n");
	char *file2 = "deeksha";

	fd = fs_open(file2);

	print_dir();
    print_FAT();


	printf("deleting hello\n");

	fs_delete("hello");

	print_dir();
	print_FAT();

	printf("writing 3 blocks of 1s on deeksha and then reading, since all blocks initialized to zero, on reading these blocks 1 should be printed \n");
	char buf[BLOCK_SIZE*2];
	memset(buf, 1, BLOCK_SIZE*2);

	fs_write(file2, fd, buf, BLOCK_SIZE*2);

   print_FAT();

	char buf2[BLOCK_SIZE*2];
    memset(buf2, 0, BLOCK_SIZE*2);    //buf to be filled with constant value specified here till the given limit.
	fs_read(file2, fd, buf2, BLOCK_SIZE*2);


	i = fs_create("hello");
	print_dir();
	print_FAT();
	unmount_fs(name);

	return 0;
}

int get_free_block(){
	//returns the next free block number
	int i, index = -1;
	int *p;
	for(i=0, p = (int *)FAT; i< DATA_BLOCKS; i++){  //checks the presence of free blocks in FAT table

			if(*p == 0){   //if the entry is initialised to zero.Implies presence of free blocks
	 			index = i;
				break;
			}
			p++;
	}
	return index;
}


int dir_create_file(char * name){
	// assumes name is null terminated
	int free_block;
	static struct dir_entry *new_entry, *p, *q;

	if ((new_entry = (struct dir_entry *)malloc(sizeof(struct dir_entry))) == NULL) { //assign space for new entry
		perror("cannot allocate new entry\n");
		return -1;
	 }

	 //printf("before: new_entry: %p, DIR: %p\n", new_entry, DIR);

	if(sb.num_used_b < DATA_BLOCKS ){

		if((free_block = get_free_block())>=0){
			new_entry->used =1;
			strncpy(new_entry->name,name,MAX_F_NAME);
			new_entry->size = 1; // allocates 1st block
			new_entry->head = free_block; // block number

			if(entries==0){
				*DIR = *new_entry;
				//printf("after: new_entry: %p, DIR: %p\n", new_entry, DIR);

			}

			q = DIR + entries;  //entries act as an offset to DIR and at this position new directory will be created

			*q = *new_entry;
			// allocates the free_block to that file in fat table , adds EOF(-1)
			*(FAT + free_block) = -1;
			sb.num_used_b ++;
			entries++; //increase the number of entries as new directory ha been created.
		}

	}
	else
		printf("error: free block cannot be found\n");

		free(new_entry);
		return 1;

}

struct dir_entry * dir_list_file(char * name){ //search operation

	int i, flag=0;//block;// = -1;
	struct dir_entry * p;
	for(i=0, p = DIR; i<entries; i++){
			//printf("p name : %s\n", p->name );
			if(strcmp(p->name, name)==0){    //if file has been found
					flag = 1;

					break;
			}
			p++;
	}
	if(flag)
		return p;
	else return NULL;
}

int fs_create(char *name){
    /*creates a new file with name name in the root directory of your file system. The file is initially empty. */
		int file;
		struct dir_entry * p;

  	p = dir_list_file(name); // RETURNS DIR entry pointer

	//file

	 if (p==NULL) {
		 dir_create_file(name);
		 // you need to create this file in directory
		 return 0;
	 }
	 else { // already exists
		 fprintf(stderr, "fs_create: file [%s] already exists\n", name);
		 return -1;
	 }
}


// returns file descriptor struct pointer
struct file_descriptor* fs_open(char *name){
/*The file specified by name will be open for r/w, and the file descriptor as a structure will get returned to the calling function. If failure, fs_open returns a NULL pointer, on SUCCESS returns a file descriptor struct that can be used to  access this file.*/
		int file , t;
		struct file_descriptor* fd;
		struct dir_entry* f;
		f = dir_list_file(name); // search file name from the directory; return an int
		//which demonstrates the file's first block; if cannot find this file, print error;
		//fd = get_free_filedes(); // find an unused file descriptor from fildes
		//initialize this file descriptor;

		if(f== NULL){
			// file does not exit so we create it
			t = fs_create(name);

			if(t<0){
				printf("file cant be created\n");
				return NULL;
			}
			f = dir_list_file(name);
			//printf("file %d\n", file );
		}

            //changed the details as now file has been created
			fd = (struct file_descriptor *)malloc(sizeof(struct file_descriptor));
			fd->used = 1;
			fd->file = (f->head);
			fd->offset = 0;
			//DIR[file].ref_cnt++; // increase references to file
			//printf("%d\n", f->file );

			return fd;
		// if cannot open, return -1; else return which file descriptor in
		//fildes it is
		//using, for example, if now this file uses fildes[3], then return 3
}

int fs_delete(char *name)
{ /*deletes the file with name 'name' from the root directory of  file system and frees all data blocks  that correspond to that file. */
	int i, block = -1;
	struct dir_entry * p, *c, *l;

	int nxt;
	int *x ;

	x = FAT + (dir_list_file(name)->head);

	while(*x != -1){
		nxt = *x;
		*x = 0;
		x = FAT + nxt;
	}
	*x = 0;

	// deletes file from the directory

	for(i=0, p = DIR; i<entries; i++){
			//printf("p name : %s\n", p->name );
			if(strcmp(p->name, name)==0){

					c =DIR+i;
					l = DIR + (entries-1);

					*c = *l;

					entries--;  //on delete, decrease the entry in directory
					return 0;
			}
			p++;
	}

	return -1;
}

void print_dir(){  //print all the entries of directory.
	struct dir_entry * p;
	int i=0;

	printf("%s\n", "printing directories" );

	for(i=0, p = DIR; i<entries; i++){
		printf("name: %s\n", p->name);
		p++;
	}

}

void print_FAT(){
	int * p;
	int i;
	p= FAT;

	printf("%s\n","current FAT table" );

	for(i=0; i< DATA_BLOCKS; i++){
		if(p[i]!=0)
			printf("block no: %d, next block: %d\n", i, p[i]);

	}

}
void init_super_block(){
	//if(sb.dir_idx==0){
		// if this is the first directory entry
		// need to initialize super_block
		sb.fat_idx = 1; // starts from first block
		sb.fat_len = MAX_FAT_SIZE;

		sb.data_idx = DISK_BLOCKS - DATA_BLOCKS ;

		sb.dir_idx = sb.fat_idx + sb.fat_len;
		sb.dir_len = sb.data_idx - sb.dir_idx;
		sb.num_used_b = 0;

}



void make_fs(char *name){
	make_disk(name);
	open_disk(name);
	//set the super block;
	//write super block to block 0 of this disk{

	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);

	memcpy(buf, &sb, sizeof(struct super_block));

	//printf("buf :%d\n", buf[1] );

	block_write(0, buf);
	//}
	close_disk();
}


void load_Directory(){

		 int size, blocks, cnt;
		 char * p;

//		 size = sb.dir_len * sizeof(int);
//		 blocks = (size - 1)/ BLOCK_SIZE + 1;

 blocks = DATA_BLOCKS -1 - MAX_FAT_SIZE;
		 // blocks: no of blocks used by fat table

		 if ((DIR = (struct dir_entry *) malloc(blocks * BLOCK_SIZE)) == NULL) {
			 perror("cannot allocate DIR\n");
			 exit(1);
			}

			char buf[BLOCK_SIZE];
			// to load directory from memory into dir structure
		 for (cnt = 0, p = (char *) DIR; cnt < blocks; ++cnt) {
			 block_read(sb.dir_idx + cnt, buf);
			 memcpy(p, buf, BLOCK_SIZE);
			 p += BLOCK_SIZE;
		 }



}

void load_FAT(){

		int size, blocks, cnt;
	  char * p;


//	 size = sb.fat_len * sizeof(int);
//	 blocks = (size - 1)/ BLOCK_SIZE + 1;

	 // blocks: no of blocks used by fat table

	 blocks = MAX_FAT_SIZE;

	 if ((FAT = (int *) malloc(blocks * BLOCK_SIZE)) == NULL) {  //allocate space to the FAT table
		 perror("cannot allocate FAT\n");
		 exit(1);
		}
		char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) FAT; cnt < blocks; ++cnt) {
			 block_read(sb.fat_idx + cnt, buf);
			 memcpy(p, buf, BLOCK_SIZE);
			 p += BLOCK_SIZE;
	 }
}



void mount_fs(char *name){
	open_disk(name);
	//load super block (similar to write super block in make_fs);
	if(block_read(0, (char *)&sb)< 0)
		printf("error in mount_fs");

	load_FAT();
	load_Directory();

}

void write_back_Directory(){

	int size, blocks, cnt;
	char * p;

	 size = sb.dir_len * sizeof(int);
	 blocks = (size - 1)/ BLOCK_SIZE + 1;

	 // blocks: no of blocks used by fat table

		char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) DIR; cnt < blocks; ++cnt) {

		 block_write(sb.dir_idx + cnt, p); //block-write(dest,src);
		 p += BLOCK_SIZE;

	 }


}

void write_back_FAT(){

	int size, blocks, cnt;
	char * p;

	 size = sb.fat_len * sizeof(int);
	 blocks = (size - 1)/ BLOCK_SIZE + 1;

	 // blocks: no of blocks used by fat table

		char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) FAT; cnt < blocks; ++cnt) {

		 block_write(sb.fat_idx + cnt, p);
		 //memcpy(p, buf, BLOCK_SIZE);
		 p += BLOCK_SIZE;

	 }


}

void unmount_fs(char *name){
	//first, close all open file descriptors (check .used value and close the file)
	//write back FAT (similar to load FAT);
	write_back_FAT();

	write_back_Directory();//similar to write back FAT);

	close_disk(name);
}
