#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------
#define CEIL(a, b) ((a % b == 0)? a / b : a / b + 1)

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  blockid_t id = IBLOCK(sb.ninodes, sb.nblocks);//first data block
  uint8_t *bit_map = (uint8_t *)malloc(sizeof(uint8_t) * BLOCK_SIZE);
  uint32_t i = BBLOCK(id); // pass boot sector and super block
  uint32_t limit = BBLOCK(sb.nblocks);
  uint32_t j = id % BPB;

  for (; i < limit; i++) {
    read_block(i, (char *)bit_map);
    for (; j < BPB; j++) {
      uint8_t id_2 = j / 8;
      uint8_t b = bit_map[id_2];
      uint8_t id_3 = j % 8;
      bool not_free = (b << id_3) & 0x80;
      if (!not_free) {
	bit_map[id_2] = (1 << (7 - id_3)) | b;
	write_block(i, (char *)bit_map);
	delete []bit_map;
	return (i - 2) * BPB + j;
      }
    }
    j = 0;
  }
  delete []bit_map;
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  uint8_t *bit_map = (uint8_t *)malloc(sizeof(uint8_t) * BLOCK_SIZE);
  blockid_t i = BBLOCK(id);
  read_block(i, (char *)bit_map);
  uint8_t j = id % BPB;
  uint8_t id_2 = j / 8;
  uint8_t b = bit_map[id_2];
  uint8_t id_3 = j % 8;
  bit_map[id_2] = (~(1 << (7 - id_3))) && b;
  write_block(i, (char *)bit_map);
  delete []bit_map;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  uint32_t inum;
  for(inum = 1; inum < (bm->sb).ninodes; inum++) { // what about inum == 0?
    struct inode *ino = get_inode(inum);
    if (ino == NULL) {
      struct inode ino; //local ino
      ino.type = type;	// how about create time etc.?
      ino.size = 0;
      put_inode(inum, &ino);
      return inum;
    } else {
      delete ino;
      ino = NULL;
    }
  }
  printf("error! exceed max inode number.\n");
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  struct inode *ino = get_inode(inum);
  // Why I don't free data blocks?
  // I think one function do one things, and remove_file should do this job
  if (ino != NULL) {
    ino->type = 0;
    ino->size = 0;
    put_inode(inum, ino);

    delete ino;
  }
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode *ino = get_inode(inum);
  if (ino == NULL) return;
  time_t  atime;
  time(&atime);
  ino->atime = atime;
  ino->ctime = atime;
  int block_num = CEIL(ino->size, BLOCK_SIZE);
  *buf_out = (char *)malloc(sizeof(char) * block_num * BLOCK_SIZE);
  *size = ino->size;
  int i = 0;

  int limit = MIN(block_num, NDIRECT);
  uint32_t offset = 0;
  for (; i < limit; i++) {
    bm->read_block(ino->blocks[i], *buf_out + offset);
    offset += BLOCK_SIZE;
  }
  
  put_inode(inum, ino);
  if (block_num <= NDIRECT) {
    delete ino;
    return;
  }
  blockid_t *indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
  bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
  for (i = 0; i < block_num - limit; i++) {
    bm->read_block(indirect_blocks[i], *buf_out + offset);
    offset += BLOCK_SIZE;
  }
  delete []indirect_blocks;
  delete ino;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  struct inode *ino = get_inode(inum);
  if (ino == NULL) return;

  int offset = 0;
  int original_block = CEIL(ino->size, BLOCK_SIZE);
  int block_num = CEIL(size, BLOCK_SIZE);
  if (block_num > (int)MAXFILE) {
    printf("\tim: error! file is too large 1\n");
    return; //panic or exit?
  }

  int limit;
  int i;
  blockid_t *indirect_blocks = NULL;
  // alloc/free blocks as needed
  if (original_block < block_num) {
    limit =MIN(block_num, NDIRECT);
    for (i = 0; i < limit - original_block; i++)
      ino->blocks[original_block + i] = bm->alloc_block();

    if (block_num > NDIRECT) {
      limit = block_num - NDIRECT;
      indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);

      if (original_block <= NDIRECT) {
	ino->blocks[NDIRECT] =  bm->alloc_block();
	i = 0;
      } else {
	i = original_block - NDIRECT;
	bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
      }
      
      for (; i < limit; i++)
	indirect_blocks[i] = bm->alloc_block();
      bm->write_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
      delete []indirect_blocks;
      indirect_blocks = NULL;
    }
  } else {
    limit = MIN(original_block, NDIRECT);
    for (i = 0; i < limit - block_num; i++) 
      bm->free_block(block_num + i);
    if (original_block > NDIRECT) {
      indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
      bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
      limit = original_block - NDIRECT;
      if (block_num > NDIRECT) {
	i = block_num - NDIRECT;
      } else {
	i = 0;
	bm->free_block(ino->blocks[NDIRECT]);
      }
      for (; i < limit; i++)
	bm->free_block(indirect_blocks[i]);
      delete []indirect_blocks;
      indirect_blocks = NULL;
    }
  }

  ino->size = size;
  limit = MIN(block_num, NDIRECT);
  for (i = 0; i < limit; i++) {
    bm->write_block(ino->blocks[i], buf + offset);
    offset += BLOCK_SIZE;
  }

  time_t  mtime;
  time(&mtime);
  ino->mtime = mtime;
  ino->ctime = mtime;
  put_inode(inum, ino);

  if (block_num <= NDIRECT) {
    delete ino;
    return;
  }

  indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
  bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
  for (i = 0; i < block_num - limit; i++) { 
    bm->write_block(indirect_blocks[i], buf + offset);
    offset += BLOCK_SIZE;
  }
  delete []indirect_blocks;
  delete ino;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *ino = get_inode(inum);
  if (ino == NULL) {
    a.type = 0;
    a.size = 0;
  } else {
    a.type = ino->type;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    a.size = ino->size;
    delete ino;
    ino = NULL;
  }
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode *ino = get_inode(inum);
  if (ino == NULL) return;
  uint32_t original_block = CEIL(ino->size, BLOCK_SIZE);
  uint32_t limit = MIN(original_block, NDIRECT);
  uint32_t id;
  for (id = 0; id <limit; id++)
    bm->free_block(id);

  if (original_block > NDIRECT) {
    blockid_t *indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
    bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
    for (id = 0; id < original_block - limit; id++)
      bm->free_block(indirect_blocks[id]);
    delete [] indirect_blocks;
    indirect_blocks = NULL;
  }
  time_t  mtime;
  time(&mtime);
  ino->mtime = mtime;
  ino->ctime = mtime;
  ino->size = 0;
  ino->type = 0;
  put_inode(inum, ino);
  free_inode(inum);
  delete ino;
}
