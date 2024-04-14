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

struct ClientRequest {
  LaptopInfo laptop;
  std::promise<LaptopInfo> prom;
};

struct ServerAddress {
  std::string ip;
  int port;
};

class LaptopFactory {
private:
  // queue of client (customer) requests
  std::queue<std::unique_ptr<ClientRequest>> erq;
  std::mutex erq_lock;
  std::condition_variable erq_cv;

  // queue of Paxos phase 2 requests
  // Scout --> ph2q --> Commander
  std::queue<std::unique_ptr<ClientRequest>> ph2q;
  std::mutex ph2q_lock;
  std::condition_variable ph2q_cv;
  
  // map of customer records
  std::mutex cr_lock;
  std::map<int, int> customer_record;

  std::map<int, ServerAddress> acceptor_map;
  bool scout_acceptor_stub_init;
  bool commander_learner_stub_init;
  std::map<int, ClientStub> scout_acceptor_stub; 
  std::map<int, ClientStub> commander_learner_stub; 
  std::vector<MapOp> smr_log;

  int last_index;
  int committed_index;
  //int primary_id;  every node can be primary node
  int factory_id;

  int numOfAcceptors = 0;

  int proposalNumber = 0;
  int promisedProposalNumber = -1;
  int acceptedProposalNumber = -1;

  std::mutex av_lock;
  Command acceptedValue;

  std::mutex slot_in_lock;
  int slot_in = 1;  // the next proposal Map slot to fill

  std::mutex slot_out_lock;
  int slot_out = 1;  // the next decision Map slot to fill

  // std::mutex proposalMap_lock;
  // std::map<int, Command> proposalMap;
  
  std::mutex decisionMap_lock;
  std::map<int, Command> decisionMap;
  



  LaptopInfo CreateRegularLaptop(CustomerRequest order, int engineer_id);
  CustomerRecord CreateCustomerRecord(CustomerRequest request);
  LogRequest CreateLogRequest(MapOp op);
  ServerConfig serverConfig;

  int ScoutBrocasting(LaptopInfo& laptop);
  void CommanderBrocasting(Command cmd);
  int AcceptPhaseBrocasting(Command cmd);
  void Learn(Command cmd);
public:
  LaptopFactory();
  void EngineerThread(std::unique_ptr<ServerSocket> socket, int id);
  void ScoutThread(int id);
  //void CommanderThread(int id);
  void AddAcceptor(int id, std::string ip, int port);
};

#endif // end of #ifndef __SERVERTHREAD_H__
