// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string content;
    extent_protocol::attr a;
    extent_protocol::status ret;
    if ((ret = ec->getattr(ino, a)) != extent_protocol::OK) {
      printf("error getting attr, return not OK\n");
      return ret;
    }
    ec->get(ino, content); 	// it always returns OK
    if (a.size < size) {
      content += std::string(size - a.size, '\0');
    } else if (a.size > size) {
      content = content.substr(0, size);
    }
    ec->put(ino, content);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, extent_protocol::types type)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;
    yfs_client::status ret;
    std::string content, inum_str;
    const std::string split1 = ":", split2 = " ";

    // the file is not found
    if ((ret = lookup(parent, name, found, ino_out)) == NOENT) {
      ec->create(type , ino_out);
      ec->get(parent, content);
      char c[100];
      sprintf(c, "%lld", ino_out);
      inum_str = c;
      inum_str = split1 + inum_str + split2;
      content += name + inum_str;
      ec->put(parent, content);
      return OK;
    } else if (ret == OK) 	// the file is found
      return EXIST;
    else 
      return ret;
    
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    // directory format *name:inum;*
    std::string content, inum_str;
    const std::string split1 = ":", split2 = " ";
    if (ec->get(parent, content) != extent_protocol::OK) { // according to code , it always returns OK;
      printf("yfs_client.cc:lookup error get, return not OK\n");
      return RPCERR;		// ??what to return?
    }
    std::string::size_type position = content.find(name), position1, position2;
    std::string tmp(name);
    if (position == content.npos || content[tmp.size() + position] != ':') {
      printf("yfs_client.cc:lookup file not exist.\n");
      return NOENT;
    }
    position1 = content.find(split1, position);
    position2 = content.find(split2, position);

    inum_str = content.substr(position1 + 1, position2);
    sscanf(inum_str.c_str(), "%lld", &ino_out); // or ostringstream?
    found = true;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string content, split1 = ":", split2 = " ";
    
    ec->get(dir, content);
    while ( !content.empty()) {
      std::string inum_str;
      yfs_client::dirent di;
      std::string::size_type position1 = content.find(split1), position2 = content.find(split2);
      di.name = content.substr(0, position1);
      inum_str = content.substr(position1 + 1, position2 - position1);
      content = content.substr(position2 + 1);
      inum ino;
      sscanf(inum_str.c_str(), "%lld", &ino); // or ostringstream?
      di.inum = ino;
      list.push_back(di);
    }
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    ec->get(ino, data);		// it always returns OK
    if (off <= data.size())
      data = data.substr(off, size);
    else 
      data = "";
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string content;
    ec->get(ino, content);
    std::string tmp(data);
    if (off > content.size()) {
      bytes_written = off  - content.size() + size; // bytes_written
      content += std::string(off - content.size(), '\0');
      content += tmp.substr(0, size);
    } else {
      bytes_written = size;
      tmp = tmp.substr(0, size);
      content.replace(off, size, tmp);
    }
    ec->put(ino, content);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    extent_protocol::attr a;
    inum ino_out;

    std::string content, inum_str;
    const std::string split1 = ":", split2 = " ";
    if (ec->get(parent, content) != extent_protocol::OK) { // according to code , it always returns OK;
      printf("yfs_client.cc:lookup error get, return not OK\n");
      return RPCERR;		// ??what to return?
    }
    std::string::size_type position = content.find(name), position1, position2;
    std::string tmp(name);
    if (position == content.npos || content[tmp.size() + position] != ':') {
      printf("yfs_client.cc:lookup file not exist.\n");
      return ENOENT;		// what is ENOENT??
    }
    position1 = content.find(split1, position);
    position2 = content.find(split2, position);

    inum_str = content.substr(position1 + 1, position2);
    sscanf(inum_str.c_str(), "%lld", &ino_out); // or ostringstream?

    ec->getattr(ino_out, a);
    if (a.type == extent_protocol::T_DIR)
      return ENOSYS;		// what does return value mean?
    
    content.erase(position, position2 - position + 1);
    ec->remove(ino_out);
    ec->put(parent, content);
    return r;
}

