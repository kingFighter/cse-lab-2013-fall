// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;
pthread_mutex_t ar_cache_mutex_client;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  pthread_mutex_lock(&ar_cache_mutex_client);
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  if (it == lock_cst.end()) {
    lock_cache_status lcs;
    lcs.status = NONE;
    lcs.revoke = false;
    lcs.retry = false;
    pthread_cond_init(&lcs.ar_threshold_cv_wait, NULL);
    pthread_cond_init(&lcs.ar_threshold_cv_retry, NULL);
    lock_cst.insert(std::make_pair<lock_protocol::lockid_t, lock_cache_status>(lid, lcs));
  }
  it = lock_cst.find(lid);
  bool loop = true;
  bool isAcquire = false;
  while (loop) {
    switch(it->second.status) {
    case NONE:
      it->second.status = ACQUIRING;
      isAcquire = true;
      loop = false;
      break;
    case FREE:
      it->second.status = LOCKED;
      loop = false;
      break;
    case ACQUIRING:
    case LOCKED:
    case RELEASING:
      while (it->second.status == LOCKED || it->second.status == ACQUIRING 
	     || it->second.status == RELEASING) 
	pthread_cond_wait(&it->second.ar_threshold_cv_wait, &ar_cache_mutex_client);
    break;
    }
  }
  
  if (isAcquire) {
    while (!(it->second.retry)) {
      pthread_mutex_unlock(&ar_cache_mutex_client);
      int r;
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      pthread_mutex_lock(&ar_cache_mutex_client);
      if (ret == lock_protocol::OK) {
	it->second.status = LOCKED;
	break;
      } else if (ret == lock_protocol::RETRY) {
	while(!it->second.retry) 
	  pthread_cond_wait(&it->second.ar_threshold_cv_retry, &ar_cache_mutex_client);
	it->second.retry = false;
      }
    }
  }
  pthread_mutex_unlock(&ar_cache_mutex_client);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&ar_cache_mutex_client);
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  bool isRelease = false;
  if (it == lock_cst.end()) {
    tprintf("ERROR: lock_client_cache relesse.\n");
    return lock_protocol::RETRY;
  }
  if (it->second.revoke) {
    it->second.revoke = false;
    isRelease = true;
  } else {
    switch(it->second.status) {
    case LOCKED:
      it->second.status = FREE;
      pthread_cond_signal(&it->second.ar_threshold_cv_wait);
      break;
    case RELEASING:
      isRelease = true;
      break;
    default:
      tprintf("ERROR: lock_client_cache release default.\n");
    }
  }
  if (isRelease) {
    pthread_mutex_unlock(&ar_cache_mutex_client);  
    int r;
    lu->dorelease(lid);
    int ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&ar_cache_mutex_client);  
    it->second.status = NONE;
    pthread_cond_broadcast(&it->second.ar_threshold_cv_wait);
  }
  pthread_mutex_unlock(&ar_cache_mutex_client);  
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  pthread_mutex_lock(&ar_cache_mutex_client);    
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  bool isRelease = false;
  switch(it->second.status) {
  case NONE:
  case ACQUIRING:
  case RELEASING:
    it->second.revoke = true;
    break;
  case FREE:
    it->second.status = RELEASING;
    isRelease = true;
    break;
  case LOCKED:
    it->second.status = RELEASING;
    break;
  }
  if (isRelease) {
    pthread_mutex_unlock(&ar_cache_mutex_client);      
    int r;
    lu->dorelease(lid);
    int ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&ar_cache_mutex_client);  
    it->second.status = NONE;
    pthread_cond_broadcast(&it->second.ar_threshold_cv_wait);
  }
  pthread_mutex_unlock(&ar_cache_mutex_client);  
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  pthread_mutex_lock(&ar_cache_mutex_client);  
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, lock_cache_status>::iterator it = lock_cst.find(lid);
  pthread_cond_signal(&it->second.ar_threshold_cv_retry);
  it->second.retry = true;
  pthread_mutex_unlock(&ar_cache_mutex_client);  
  return ret;
}



