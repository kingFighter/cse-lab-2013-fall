// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "lock_client_cache.h"

class extent_client {
 private:
  rpcc *cl;
  class caches_status {
  public:
    int dirty;
    int invalid;
  }; 
  std::map<extent_protocol::extentid_t, std::string> caches;
  std::map<extent_protocol::extentid_t, caches_status> caches_st; /* 0: ; 1: dirty */
  std::map<extent_protocol::extentid_t, extent_protocol::attr> caches_attr;
 public:
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  void flush(extent_protocol::extentid_t eid);
};

class lock_release_user_im : public lock_release_user {
 public:
 lock_release_user_im(extent_client *_ec):ec(_ec){}
  virtual void dorelease(lock_protocol::lockid_t lt) {ec->flush(lt);}
  virtual ~lock_release_user_im(){};
 private:
  extent_client *ec;
};
#endif 

