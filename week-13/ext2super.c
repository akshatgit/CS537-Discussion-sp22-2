/*
 * ext2super.c
 *
 * Reads the super-block from a Ext2 floppy disk.
 *
 * Questions?
 * Emanuele Altieri
 * ealtieri@hampshire.edu
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/ext2_fs.h>

#define BASE_OFFSET 1024                   /* locates beginning of the super block (first group) */
#define FD_DEVICE "/dev/fd0"              /* the floppy disk device */

static unsigned int block_size = 0;        /* block size (to be calculated) */

int main(void)
{
	struct ext2_super_block super;
	int fd;

	/* open floppy device */

	if ((fd = open(FD_DEVICE, O_RDONLY)) < 0) {
		perror(FD_DEVICE);
		exit(1);  /* error while opening the floppy device */
	}

	/* read super-block */

	lseek(fd, BASE_OFFSET, SEEK_SET); 
	read(fd, &super, sizeof(super));
	close(fd);

	if (super.s_magic != EXT2_SUPER_MAGIC) {
		fprintf(stderr, "Not a Ext2 filesystem\n");
		exit(1);
	}
		
	block_size = 1024 << super.s_log_block_size;

	printf("Reading super-block from device " FD_DEVICE ":\n"
	       "Inodes count            : %u\n"
	       "Blocks count            : %u\n"
	       "Reserved blocks count   : %u\n"
	       "Free blocks count       : %u\n"
	       "Free inodes count       : %u\n"
	       "First data block        : %u\n"
	       "Block size              : %u\n"
	       "Blocks per group        : %u\n"
	       "Inodes per group        : %u\n"
	       "Creator OS              : %u\n"
	       "First non-reserved inode: %u\n"
	       "Size of inode structure : %hu\n"
	       ,
	       super.s_inodes_count,  
	       super.s_blocks_count,
	       super.s_r_blocks_count,     /* reserved blocks count */
	       super.s_free_blocks_count,
	       super.s_free_inodes_count,
	       super.s_first_data_block,
	       block_size,
	       super.s_blocks_per_group,
	       super.s_inodes_per_group,
	       super.s_creator_os,
	       super.s_first_ino,          /* first non-reserved inode */
	       super.s_inode_size);
	
	exit(0);
} /* main() */
