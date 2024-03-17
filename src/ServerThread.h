#ifndef __SERVERTHREAD_H__
#define __SERVERTHREAD_H__

#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <queue>

#include "Messages.h"
#include "ServerSocket.h"
#include "ClientStub.h"

struct AdminRequest {
  LaptopInfo laptop;
  std::promise<LaptopInfo> prom;
};

struct ServerAddress {
  std::string ip;
  int port;
};

class LaptopFactory {
private:
  std::queue<std::unique_ptr<AdminRequest>> erq;
  std::mutex erq_lock;
  std::condition_variable erq_cv;

  std::mutex cr_lock;
  std::map<int, int> customer_record;
  std::map<int, ServerAddress> admin_map;
  bool admin_stub_init;
  std::map<int, ClientStub> admin_stub;
  std::vector<MapOp> smr_log;

  int last_index;
  int committed_index;
  int primary_id;
  int factory_id;

  LaptopInfo CreateRegularLaptop(CustomerRequest order, int engineer_id);
  CustomerRecord CreateCustomerRecord(CustomerRequest request);
  LogRequest CreateLogRequest(MapOp op);

  void PFA(LaptopInfo& laptop);

public:
  void EngineerThread(std::unique_ptr<ServerSocket> socket, int id);
  void AdminThread(int id);
  void AddAdmin(int id, std::string ip, int port);
};

#endif // end of #ifndef __SERVERTHREAD_H__
