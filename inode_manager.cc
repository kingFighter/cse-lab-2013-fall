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

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  blockid_t id = IBLOCK(sb.ninodes, sb.nblocks);
  for (; id < sb.nblocks; id++)
    if (!bit_map[id]) {
      bit_map[id] = true;
      return id;
    }

  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  bit_map[id] = false;
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
  bit_map = new bool[sb.nblocks];
  uint32_t i = 0;

  for (; i < sb.nblocks; i++)
    bit_map[i] = false;
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

  return;
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
  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return;//panic or exit?
  }
  struct inode *ino = get_inode(inum);

  uint32_t block_num = (ino->size % BLOCK_SIZE == 0)? ino->size / BLOCK_SIZE : ino->size / BLOCK_SIZE + 1;

  *buf_out = (char *)malloc(sizeof(char) * block_num * BLOCK_SIZE);

  *size = ino->size;
  uint32_t i = 0;

  uint32_t limit = block_num > NDIRECT ? NDIRECT : block_num;
  uint32_t offset = 0;
  for (; i < limit; i++) {
    bm->read_block(ino->blocks[i], *buf_out + offset);
    offset += BLOCK_SIZE;
  }

  if (limit < NDIRECT)
    return;
  blockid_t *indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
  bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
  for (i = 0; i < block_num - limit; i++) {
    bm->read_block(indirect_blocks[i], *buf_out + BLOCK_SIZE);
    offset += BLOCK_SIZE;
  }
  delete []indirect_blocks;
  delete ino;
  return;
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
  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return;//panic or exit?
  }
  struct inode *ino = get_inode(inum);
  int offset = 0;
  uint32_t original_block = (ino->size % BLOCK_SIZE == 0)? ino->size / BLOCK_SIZE : ino->size / BLOCK_SIZE + 1;
  uint32_t max_block = (size % BLOCK_SIZE == 0)? size / BLOCK_SIZE : size / BLOCK_SIZE + 1;
  if (max_block > MAXFILE) {
    printf("\tim: error! file is too large 1\n");
    return; //panic or exit?
  }
  uint32_t limit;
  uint32_t i;
  blockid_t *indirect_blocks;
  if (original_block < max_block) {
    limit = max_block > NDIRECT ? NDIRECT : max_block;  
    for (i = 0; i < limit - original_block; i++)
      ino->blocks[original_block + i] = bm->alloc_block();
    if (original_block <= NDIRECT && max_block > NDIRECT) {
      indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
      for (i = 0; i < NINDIRECT; i++)
	indirect_blocks[i] = bm->alloc_block();
      bm->write_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
      delete []indirect_blocks;
    }
  } else {
    for (i = 0; i < original_block - max_block; i++) 
      bm->free_block(max_block + i);
    if (original_block > NDIRECT && max_block <= NDIRECT) {
      indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
      bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
      for (i = 0; i < NINDIRECT; i++)
	bm->free_block(indirect_blocks[i]);
      delete []indirect_blocks;
    }
  }

  ino->size = size;
  put_inode(inum, ino);
  limit = max_block > NDIRECT ? NDIRECT : max_block;  
  for (i = 0; i < limit; i++) {
    bm->write_block(ino->blocks[i], buf + offset);
    offset += BLOCK_SIZE;
  }

  if (limit < NDIRECT)
    return;
  indirect_blocks = (blockid_t *)malloc(sizeof(blockid_t) * NINDIRECT);
  bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
  for (i = 0; i < max_block - limit; i++) { 
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
  a.type = ino->type;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;
  a.size = ino->size;
  delete ino;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  
  return;
}
