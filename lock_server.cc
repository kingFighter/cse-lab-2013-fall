// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t ar_mutex;
pthread_cond_t ar_threshold_cv;

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("acquire request from clt %d\n", clt);

  pthread_mutex_lock(&ar_mutex);
  if (lock_st.count(lid) <= 0) {
    lock_status ls = LOCKED;
    lock_st.insert(std::make_pair(lid,ls));
  } else {
    while(lock_st[lid] == LOCKED) 
      pthread_cond_wait(&ar_threshold_cv, &ar_mutex);

    lock_st[lid] = LOCKED;
  }
  pthread_mutex_unlock(&ar_mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("release request from clt %d.\n", clt);
  pthread_mutex_lock(&ar_mutex);
  if (lock_st.count(lid) <= 0)
    ret = lock_protocol::NOENT;
  else {
    lock_st[lid] = FREE;
    pthread_cond_signal(&ar_threshold_cv);//pthread_cond_broadcast(&ar_threshold_cv);
  }
  pthread_mutex_unlock(&ar_mutex);
  return ret;
}
