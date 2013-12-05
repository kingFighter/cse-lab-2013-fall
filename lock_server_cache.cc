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
      ret = lock_protocol::OK;	// ??
      isRevoke = true;
    break;
  case LOCKED_WAIT:
    (it->second).ocids.insert(id);
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
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

