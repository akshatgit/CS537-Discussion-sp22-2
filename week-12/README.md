# COMP SCI 537 Discussion Week 12

The plan for this week:
- Detecting JPEG images.
- Inodes
- Reading Inodes
- Reading Disk Image

Primarily, this project is about iterating through inodes(using starter code), identifying inodes & blocks of jpeg files and storing 2 copies in output folder.

## Detecting JPEG images

To detect inodes with images, scan all inodes that represent regular files and check if the first data block of the inode contains the jpg magic numbers: **FF D8 FF** E0 or **FF D8 FF** E1 or **FF D8 FF** E8.
Notice last 3 possibilities are E0, E1 and E8 - which is what following code is looking for,

```C
int is_jpg = 0;
if (buffer[0] == (char)0xff && buffer[1] == (char)0xd8 && buffer[2] == (char)0xff &&
    (buffer[3] == (char)0xe0 || buffer[3] == (char)0xe1 || buffer[3] == (char)0xe8)) 
{
    is_jpg = 1;
}
```

## Inode

To start off, you need to look at structure of inode on disk and look up relevant variables for achieving goals of this project.
- i_mode - An inode can represent a regular file, a directory, a symbolic link, and other file types. This variable represents field type. See S_ISDIR and S_ISREG functions to determine whether file is directory or regular file.
- i_size stores size of the regular file.
- i_links_count field identifies whether this inode is used or not. Since we need to copy deleted and undeleted files both, this field can be used as a mid-project checkpoint to first restore undeleted files and then deleted.
- i_block is where you copy data from.
```C
/*
 * Structure of an inode on the disk
 */
struct ext2_inode {

	__u16	i_mode;		/* File mode */
    ...
	__u32	i_size;		/* Size in bytes */
    ...
    __u16	i_links_count;	/* Links count */
    ...
	__u32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
    ...
};
```

## Reading Inodes
How can one read inodes? Using starter code. We suggest reading [runscan.c](https://github.com/himanshusagar/p5/blob/main/runscan.c) before starting P5. This file shows how to browse through disk image and iterate over inodes using 
read_inode function inside [read_ex2.c](https://github.com/himanshusagar/p5/blob/main/read_ext2.c#L100). First off, we'll look at read_inode function and observe what argument it takes and what it returns.

```C

/* read an inode with specified inode number and group number */
void read_inode(
        int                          fd,        /* the disk image file descriptor */
        off_t 			     offset,    /* offset to the start of the inode table */
        int                          inode_no,  /* the inode number to read  */
        struct ext2_inode            *inode   /* where to put the inode */); 

```
An ext2 file system is partitioned into multiple cylinder/block groups. In each block group, there is an inode table. The inode table is basically an array of inodes.
The inode table does not store inode numbers, hence you should track the inode numbers. Inode number starts from 1. If there are 100 inodes per group, 
then the first inode table holds inode number 1 to 100, the second holds 101 to 200, and so on. In other words, the first inodeTable[0..99] array stores inodes numbered from 1 to 100.

## Reading Disk Image
Now, how can one reach inodes? Using starter code. See runscan.c. 
It browses ext2_super_block and finds inode table and then browses through each inode entry one by one.
As you may, we've already provided code that does this heavy-lifting in runscan.c. 

```C
 for (unsigned int i = 0; i < inodes_per_block; i++) 
 {
            printf("inode %u: \n", i);
            struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
            read_inode(fd, start_inode_table, i, inode);
            printf("number of blocks %u\n", inode->i_blocks>>1);	// the alignment is 1 byte off for new ext2; does not affect the block pointers
            printf("Is directory? %s \n Is Regular file? %s\n",
                S_ISDIR(inode->i_mode) ? "true" : "false",
                S_ISREG(inode->i_mode) ? "true" : "false");
```

## Copying Data from Blocks.

EXT2_NDIR_BLOCKS is 12. We've 12 direct pointers to data. For starters, you can think about restoring/copying files which are less than 12 KB in size.
This file will be store in 12 direct pointers and can easily be restored by copying data from these blocks to your output directory. One easy way to do so is by opening a new file pointer(maybe in append mode?)
with name "file-<inode_number>.jpg" and appending bytes from inode blocks to your file pointer.

Once you're able to restore files less than 12 KB in size, you can think about using indirect pointers. 
Each block is of 1024 B. It is indexed with a 4 B block number. This gives 1024/4 = 256 pointers.
An indirect block is simply contains an array of 256 pointers.

```C
for(unsigned int i=0; i<EXT2_N_BLOCKS; i++)
{       
    if (i < EXT2_NDIR_BLOCKS)     /* direct blocks */
	    printf("Block %2u : %u\n", i, inode->i_block[i]);
        .....
```

### Exercise

Can you see why we have whole hierarchy of single, double and triple indirect blocks? 
If yes, what is the largest file size one can store with
- just single indirect blocks?
- single + double indirect blocks?
- single + double + triple indirect blocks?
