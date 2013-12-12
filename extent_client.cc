// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  caches.clear();
  caches_st.clear();
  caches_attr.clear();
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (caches_attr.count(eid) != 0) 
    attr = caches_attr[eid];
  else {
    ret = cl->call(extent_protocol::getattr, eid, attr);
    caches_attr[eid] = attr;
  }
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::create, type, id);
  caches_attr[id].type = type;
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (caches.count(eid) != 0) {
    buf = caches[eid];
  } else {
    // Your lab3 code goes here
    ret = cl->call(extent_protocol::get,eid, buf);
    caches[eid] = buf;
  }
  time_t atime;
  time(&atime);
  caches_attr[eid].atime = atime;
  caches_attr[eid].ctime = atime;
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int nothing = 1;

  if (caches[eid].compare(buf)) {
    caches[eid] = buf;
    caches_st[eid] = 1;
  }
  caches_attr[eid].size = buf.size();
  time_t mtime;
  time(&mtime);
  caches_attr[eid].mtime = mtime;
  caches_attr[eid].ctime = mtime;
  // ret = cl->call(extent_protocol::put, eid, buf, nothing);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int nothing = 1;
  caches.erase(eid);
  caches_st.erase(eid);
  caches_attr.erase(eid);
  // ret = cl->call(extent_protocol::remove, eid, nothing);
  return ret;
}


