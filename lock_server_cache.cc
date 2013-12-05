// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

pthread_mutex_t ar_cache_mutex;
lock_server_cache::lock_server_cache():
  nacquire(0)

{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  tprintf("stat request\n");
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&ar_cache_mutex);
  bool isRevoke = false;
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  if (it == lock_cst.end()) {
    lock_cache_status lcs;
    lcs.status = FREE;
    lcs.cid = "";
  }
  switch((it->second).status) {
  case FREE:
    (it->second).status = LOCKED;
    (it->second).cid = id;
    ret =  lock_protocol::OK;
    break;
  case LOCKED:
      ((it->second)).status = LOCKED_WAIT;
      (it->second).ocids.insert(id);
      it->second.next_cid = id;
      ret = lock_protocol::RETRY;	
      isRevoke = true;
    break;
  case LOCKED_WAIT:
    std::set<std::string>::iterator sit = it->second.ocids.find(it->second.next_cid);
    if (sit != it->second.ocids.end()) {
      it->second.cid = it->second.next_cid;
      it->second.ocids.erase(sit);
      if (it->second.ocids.empty()) {
	it->second.status = LOCKED;
	it->second.next_cid = "";
      } else {
	it->second.next_cid = *(it->second.ocids.begin());
	isRevoke = true;
      }
      ret = lock_protocol::OK;
    } else {
      (it->second).ocids.insert(id);
      ret = lock_protocol::RETRY;
    }
    break;
  }
  if (isRevoke) {
    handle holder((it->second).cid);
    rpcc * cl = holder.safebind();
    if (cl) {
      pthread_mutex_unlock(&ar_cache_mutex);
      int r = cl->call(rlock_protocol::revoke, lid, r);
      pthread_mutex_lock(&ar_cache_mutex);
      if (r != rlock_protocol::OK) 
	tprintf("lock_server_cache acquire revoke failed\n");
    } else {
      tprintf("lock_server_cache acquire cannot safe bind");
    }
  }
  pthread_mutex_unlock(&ar_cache_mutex);
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&ar_cache_mutex);  
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  bool isRetry = false;
  if (it == lock_cst.end()) {
    tprintf("ERROR in lock_server_cache release.\n");
  }
  else {
    switch((it->second).status) {
    case FREE:
      tprintf("ERROR: lock_sersver_cache release(FREE).\n");
      break;
    case LOCKED:
      (it->second).status = FREE;
      (it->second).cid = "";
      if (!(it->second).ocids.empty())
	tprintf("ERROR: lock_server_cache release(LOCKED).\n");
      break;
    case LOCKED_WAIT:
      isRetry = true;
      it->second.cid = "";
      break;
    }
  }

  if (isRetry) {
    handle h(it->second.next_cid);
    rpcc * cl = h.safebind();
    if (cl) {
      pthread_mutex_unlock(&ar_cache_mutex);  
      int rs = cl->call(rlock_protocol::retry, lid, r);
      pthread_mutex_lock(&ar_cache_mutex);  
      if (rs != rlock_protocol::OK) 
	tprintf("ERROR: lock_server_cache release.\n");
    } else {
      tprintf("lock_server_cache release cannot safe bind");
    }
  }
  pthread_mutex_unlock(&ar_cache_mutex);  
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

