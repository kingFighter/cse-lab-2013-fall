#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <set>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  enum Status {FREE, LOCKED, LOCKED_WAIT}; /* LOCKED: NO other client waiting, LOCKED_WAIT: Others clients waiting.*/
  class lock_cache_status {
  public:
    lock_cache_status(){}
    Status status;
    std::string cid; 		/* owner client id */
    std::set<std::string> ocids; /* other clients waiting for locks */
  };
  std::map<lock_protocol::lockid_t, lock_cache_status> lock_cst;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
