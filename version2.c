/*
OS PROJECT:

Version_2
Efficient Directory Implementation
with functions for directory creation
and efficient directory search.

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
#define MAX_F_NAME 32
#define DATA_BLOCKS 2560
#define MAX_FAT_SIZE 3//(no. of data blocks *size of int) /BLOCK_SIZE = 2.5 blocks
#define MAX_SUB_DIRS 5 // max sub directories
#define MAX_FILES 5
#define DIR_BLOCKS 852
//Blocks division: 1(SB) + 3(FAT) + 1704(FILES) + 852(DIRS) + 2560(DATA_BLOCKS)


struct super_block
{
	 int version;
	  /* FAT information where it starts on disk*/
	 int fat_idx;
	 int fat_len;
	 /* files information */
	 int files_idx;
	 int files_len;
	 /* directory information*/
	 int dir_idx;
	 int dir_len;

	 /* data information */
	 int data_idx;

	 //int num_used_b;
	 int num_used_b;
};

struct directory{
	// implements the directory as a node
 	int used;
	char name[MAX_F_NAME+1]; // name of directory
	struct directory* parent;
	struct directory* sub_dir[MAX_SUB_DIRS];// pointers to sub
	struct file_entry* files[MAX_FILES];
};

struct file_entry //it stores the information of a file
{
 int used;
 char name[MAX_F_NAME + 1]; //file name
 int size;//file size
 int head; //first data block of this file
 //int ref_cnt; //how many file descriptors this entry is using, if ref_cnt > 0 we
 //cannot delete this file
};

struct file_descriptor
{
 int used; // file descriptor is in use
 int file; // the first block of current file who uses this file descriptor
 int offset;
};

struct super_block sb;
//struct file_descriptor fildes[MAX_FILDES]; //MAX_FILDES=32
int *FAT; // a queue
struct file_entry *FILE_ENTRIES ; // a queue of file file_entries_cnt
struct directory *DIRS; // queue  of directories

int file_entries_cnt = 0; // no of total file_entries_cnt in dir
int dirs_cnt = 0;

void init_super_block();
void print_FAT();
void make_fs(char *);
void load_Files();
void load_Directory();
void load_FAT();
void mount_fs(char *);
void write_back_Directory();
void write_back_FAT();
void write_back_Files();
void unmount_fs(char *);

void print_dir_entries(){
	struct directory * p;
	int i=0;
	printf("current directory entries\n");
	for(i=0, p = DIRS; i<dirs_cnt; i++){
		printf("name: %s\n", p->name);
		p++;
	}

}

struct directory* get_free_dir(){
	int i;
	struct directory * p;
	for(i=0, p = DIRS; i<DIR_BLOCKS; i++){
			if(p->used==0)
				return p;
			p++;
	}
	return NULL;
}

// uses recursion
struct directory* search(char path[][MAX_F_NAME], int len, struct directory * in){

	int flag, i;

	if(in!=NULL && len>0){

		if(len==1){
			if(strcmp(in->name, path[0])==0)
				return in;
			else return NULL;
		}

			for(i=0; i<MAX_SUB_DIRS; i++)
			{
				if( (in->sub_dir[i]!= NULL) && (strcmp( (in->sub_dir[i]->name), path[1])==0)){
						flag =1; // exists
						break;

				}
			}
				if(flag){
					// dir exists
					if(len>1)
						return search( path + 1, len-1, in->sub_dir[i] );
					else
						return in->sub_dir[i];

				}
				else // no such dir
					return NULL;

		}
		else // invalid root
			return NULL;
}


// eg path = {"a", "b"} creates directory b in directory a if it exists
int	create_dir(char path [][MAX_F_NAME], int len ){
	int i;
	char name[MAX_F_NAME];

	strncpy(name, path[len-1], MAX_F_NAME);

	struct directory *parent, *freed, *freep;
	if(len!=1){// else creation of root

		 parent = search(path, len-1 , DIRS);

		if(parent == NULL){
			  printf("Such path does not exist\n");
				return -1;
		}
		else{
				if((freed= get_free_dir())== NULL){
					  printf("DIRS full\n");
						return -1;
				}
				// parent should have free sub_dir

				for(i=0; i< MAX_SUB_DIRS; i++){

					if((parent->sub_dir[i]==NULL) || (parent->sub_dir[i]->used) !=1){
							parent->sub_dir[i]= freed;
							break;
					}

				}
				if(i==MAX_SUB_DIRS){
						printf("Parent full\n" );
						return -1;
				}
			}
	}
	else{
				freed = DIRS;
	}
	struct directory newd;
	newd.used = 1;
	strncpy(newd.name,name,MAX_F_NAME);

	for(i=0; i<MAX_SUB_DIRS; i++)
		newd.sub_dir[i] = NULL;

	for(i=0; i<MAX_FILES; i++)
			newd.files[i] = NULL;

	if(len==0){
				newd.parent = NULL;
				printf("%s\n","root created" );
	}
	else
				newd.parent = parent;

	*freed = newd;

	printf("%s\n","entry created" );
	dirs_cnt++;
	return 1;
}

int main(){

	char* name = "MyDisk.txt";
	int i;
	make_fs(name);
	mount_fs(name);

	char path [][MAX_F_NAME]= {"a"} ;

	create_dir(path, 1);
	char path2 [][MAX_F_NAME]= {"a", "b"} ;

	create_dir( path2 , 2 );
	print_dir_entries();
	struct directory * d = search(path2, 2, DIRS);
	printf("%s  %s\n", "search for b returned", d->name);

	unmount_fs(name);

	return 0;
}

void print_FAT(){
	int * p;
	int i;
	p= FAT;
	for(i=0; i< DATA_BLOCKS; i++){
		if(p[i]!=0)
			printf("block no: %d, next block: %d\n", i, p[i]);

	}
}

void init_super_block(){

		// need to initialize super_block
		sb.fat_idx = 1; // starts from first block
		sb.fat_len = MAX_FAT_SIZE;

		sb.data_idx = DISK_BLOCKS - DATA_BLOCKS ;

		sb.dir_idx = DATA_BLOCKS - DIR_BLOCKS;
		sb.dir_len = DIR_BLOCKS;

		sb.files_idx = sb.fat_idx + sb.fat_len;
		sb.files_len = sb.dir_idx - sb.files_idx;

		sb.num_used_b = 0;
}



void make_fs(char *name){
	make_disk(name);
	open_disk(name);
	init_super_block();

	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);
	memcpy(buf, &sb, sizeof(struct super_block));
	block_write(0, buf);

	close_disk();
}


void load_Files(){

		 int size, blocks, cnt;
		 char * p;

 	 blocks = DATA_BLOCKS -1 - MAX_FAT_SIZE;
		 // blocks: no of blocks used by fat table

		 if ((FILE_ENTRIES = (struct file_entry *) malloc(blocks * BLOCK_SIZE)) == NULL) {
			 perror("cannot allocate FILE_ENTRIES\n");
			 exit(1);
			}

			char buf[BLOCK_SIZE];
			// to load files from memory
		 for (cnt = 0, p = (char *) FILE_ENTRIES; cnt < blocks; ++cnt) {
			 block_read(sb.files_idx + cnt, buf);
			 memcpy(p, buf, BLOCK_SIZE);
			 p += BLOCK_SIZE;
		 }
}

void load_Directory(){

		 int size, blocks, cnt;
		 char * p;

		 // blocks: no of blocks used by directory
 	   blocks = sb.dir_len;

		 if ((DIRS = (struct directory *) malloc(blocks * BLOCK_SIZE)) == NULL) {
			 perror("cannot allocate directories\n");
			 exit(1);
			}

		 char buf[BLOCK_SIZE];
			// to load files from memory
		 for (cnt = 0, p = (char *) DIRS; cnt < blocks; ++cnt) {
			 block_read(sb.files_idx + cnt, buf);
			 memcpy(p, buf, BLOCK_SIZE);
			 p += BLOCK_SIZE;
		 }
}


void load_FAT(){

		int size, blocks, cnt;
	  char * p;

	 blocks = MAX_FAT_SIZE;

	 if ((FAT = (int *) malloc(blocks * BLOCK_SIZE)) == NULL) {
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
	//open_error_handling;
	open_disk(name);
	//load super block

	if(block_read(0, (char *)&sb)< 0)
		printf("error in mount_fs");

	load_FAT();
	load_Files();
	load_Directory();
}

void write_back_Directory(){

	int size, blocks, cnt;
	char * p;

	  blocks = sb.dir_len ;

	char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) DIRS; cnt < blocks; ++cnt) {

		 block_write(sb.files_idx + cnt, p);
		 //memcpy(p, buf, BLOCK_SIZE);
		 p += BLOCK_SIZE;
	 }
}


void write_back_Files(){

	int size, blocks, cnt;
	char * p;

	 blocks = sb.files_len;

	char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) FILE_ENTRIES; cnt < blocks; ++cnt) {

		 block_write(sb.files_idx + cnt, p);
		 //memcpy(p, buf, BLOCK_SIZE);
		 p += BLOCK_SIZE;
	 }
}

void write_back_FAT(){

	int size, blocks, cnt;
	char * p;

	 size = sb.fat_len * sizeof(int);
	 blocks = (size - 1)/ BLOCK_SIZE + 1;

	char buf[BLOCK_SIZE];

	 for (cnt = 0, p = (char *) FAT; cnt < blocks; ++cnt) {

		 block_write(sb.fat_idx + cnt, p);
		 //memcpy(p, buf, BLOCK_SIZE);
		 p += BLOCK_SIZE;
	 }
}

void unmount_fs(char *name){

	write_back_FAT();
	write_back_Files();
	write_back_Directory();

	free(FILE_ENTRIES);
	free(DIRS);
	close_disk(name);
}
