#include "ServerThread.h"

#include <iostream>
#include <memory>

#include "ServerStub.h"

LaptopInfo LaptopFactory::CreateRegularLaptop(CustomerRequest order,
                                              int engineer_id) {
  LaptopInfo laptop;
  laptop.CopyOrder(order);
  laptop.SetEngineerId(engineer_id);
  laptop.SetAdminId(-1);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));


  std::promise<LaptopInfo> prom;
  std::future<LaptopInfo> fut = prom.get_future();

  std::unique_ptr<ClientRequest> req =
      std::unique_ptr<ClientRequest>(new ClientRequest);
  req->laptop = laptop;
  req->prom = std::move(prom);
  

  erq_lock.lock();
  erq.push(std::move(req));
  erq_cv.notify_one();
  erq_lock.unlock();

  laptop = fut.get();

  return laptop;
}

CustomerRecord LaptopFactory::CreateCustomerRecord(CustomerRequest request) {
  CustomerRecord record;
  int customer_id = request.GetCustomerId();
  int last_order = -1;
  {
    std::lock_guard<std::mutex> lock(cr_lock);
    if (customer_record.find(customer_id) != customer_record.end()) {
      last_order = customer_record[customer_id];
    } else {
      last_order = -1;
      customer_id = -1;
    }
  }
  record.SetCustomerRecord(customer_id, last_order);
  return record;
}

void LaptopFactory::EngineerThread(std::unique_ptr<ServerSocket> socket,
                                   int id) {
  int engineer_id = id;
  int request_type;
  CustomerRequest request;
  CustomerRecord record;
  LogRequest log_req;
  LaptopInfo laptop;

  ServerStub stub;

  stub.Init(std::move(socket));
  
  // before sending a message, the sender sends an int identity to the engineer
  // identity = 0 means that this engineer is talking to a client (customer)
  //              in this case, the message received is a CustomerRequest type
  // identity = 1 means that this engineer is talking to a server
  //              in this case, the message received is a LogRequest type
  // identity = 2 means that this engineer is an Acceptor, receving PaxosMsg from and sending PaxosMsg to Scout or Commander
  //              in this case, this engineer receives PaxosMsg from and sends PaxosMsg to Scout or Commander
  int identity = stub.ReceiveIndentity();
  if (identity == 0) { // talking with customer (client)
    stub.SendServerConfig(serverConfig);
    while (true) {
      request = stub.ReceiveRequest();
      if (!request.IsValid()) {
        std::cout<<"request invalid"<<std::endl;
        break;
      }
      request_type = request.GetRequestType();
      switch (request_type) {
      case 1:
        laptop = CreateRegularLaptop(request, engineer_id);
        stub.SendLaptop(laptop);
        break;
      case 2:
        record = CreateCustomerRecord(request);
        stub.ReturnRecord(record);
        break;
      default:
        std::cout << "Undefined laptop type: " << request_type << std::endl;
      }
    }
  } else if (identity == 1) { // IFA
    // here, the engineer keeps opening to the server who tries to send message to this engineer
    while (true) {
      log_req = stub.ReceiveLogRequest();
      if (!log_req.IsValid()) {
        std::cout << "Invalid log request" << std::endl;
        break;
      }

      // if (primary_id != log_req.GetFactoryId()) {
      //   primary_id = log_req.GetFactoryId();
      // }

      MapOp op = log_req.GetMapOp();
      smr_log.emplace_back(op);
      last_index = log_req.GetLastIndex();
      committed_index = log_req.GetCommittedIndex();
      if (committed_index >= 0) {
        std::lock_guard<std::mutex> lock(cr_lock);
        MapOp to_apply = smr_log[committed_index];
        customer_record[to_apply.GetArg1()] = to_apply.GetArg2();
      }

      LogResponse resp;
      resp.SetFactoryId(factory_id);
      stub.ReturnLogResponse(resp);
    }
  } else if (identity==2) { // Acceptor, contacting with Scout
    std::cout<<"Acceptor-Scout connection Initilizd";
    while (true) {
      PaxosMsg msg = stub.ReceivePaxosMsg();
      if (msg.getPhase()<0) { //this will happen when scout's process shut down
        std::cout << "Lost Contact with proposer(scout)" << std::endl;
        break;
      }
      
      if (msg.getPhase()==1) { // p1a msg
        PaxosMsg p1bMsg = PaxosMsg(2); // p1b msg
        p1bMsg.setAgree(1);
        if (!stub.SendPaxosMsg(p1bMsg)) {
          std::cout<<"p1bMsg failed to reach proposer(scout)"<<std::endl;
        }
      } else {
        std::cout<<"unknown phase";
      }
      // receive PaxosMsg
      // if 

      // p1a (prepareMsg) receive
      // p1b (promise or reject) send out

      // p2a (acceptMsg) receive (only when giving )
      // p2b ()
    }
  } else if (identity==3){ // Acceptor/ learner, contacting with Commander
    std::cout<<"Acceptor-Scout connection Initilizd";
    while (true) {
      PaxosMsg msg = stub.ReceivePaxosMsg(); // p2a message
      if (msg.getPhase()<0) { // this will happen when commander's process shut down
        std::cout << "Lost Contact with proposer(commander)" << std::endl;
        break;
      }

      if (msg.getPhase()==3) { // p2a msg
        PaxosMsg p1bMsg = PaxosMsg(4); // p1b msg
        p1bMsg.setAgree(1);
        if (!stub.SendPaxosMsg(p1bMsg)) {
          std::cout<<"p2bMsg failed to reach proposer(commander)"<<std::endl;
        }
      } else {
        std::cout<<"unknown phase";
      }
    }
  } else {
    std::cout << "Undefined identity: " << identity << std::endl;
  }
}

LogRequest LaptopFactory::CreateLogRequest(MapOp op) {
  LogRequest request;
  request.SetFactoryId(factory_id);
  request.SetLastIndex(last_index);
  request.SetCommittedIndex(committed_index);
  request.SetMapOp(op);
  return request;
}

/**
 * @brief Send out p1a massage, receive p1b message. If majority of acceptors give promise, put a p2 request to commander
 * 
 * @param laptop 
 */
void LaptopFactory::ScoutBrocasting(LaptopInfo &laptop) {
  int promisedNum = 0;

  // establish connections to all peers (including itself)
  if (!scout_acceptor_stub_init) {
    for (auto &acceptor : acceptor_map) {
      const ServerAddress &addr = acceptor.second;
      int ret = scout_acceptor_stub[acceptor.first].Init(addr.ip, addr.port);
      
      // if ret==0
      if (!ret) {
        std::cout << "Failed to connect to admin (peer) " << acceptor.first << std::endl;
        scout_acceptor_stub.erase(acceptor.first);
      } else {
        scout_acceptor_stub[acceptor.first].Identify(2);
      }
    }
    scout_acceptor_stub_init = true;
  }

  MapOp op;
  op.SetMapOp(1, laptop.GetCustomerId(), laptop.GetOrderNumber());
  smr_log.emplace_back(op);
  last_index = smr_log.size() - 1;

  // below is how the primary node sends LogRequest to all the peers.
  // we can whipe it for now

  LogRequest request = CreateLogRequest(op);

  // definition of acceptor_stub:   std::map<int, ClientStub> acceptor_stub;
  for (auto iter = scout_acceptor_stub.begin(); iter != scout_acceptor_stub.end();) {
    PaxosMsg paxosMsg = PaxosMsg(1);
    // LogResponse resp = iter->second.BackupRecord(request);

    // send PaxosMsg to all acceptors
    if (!iter->second.sendPaxosMsg(paxosMsg)) {
      std::cout << "Failed to send PaxosMsg to Acceptor, Acceptor serverId: "<<iter->first<<std::endl;
      iter = scout_acceptor_stub.erase(iter);
      break;
    } else {
      PaxosMsg p1bMsg;
      p1bMsg = iter->second.receivePaxosMsg();
      if (p1bMsg.isAgree()) {
        promisedNum ++;
      }
      ++iter;
    }
  }
  
  if (promisedNum > numOfAcceptors/2) { // majority of acceptors primised
    // for temp use
    std::unique_ptr<ClientRequest> req =
      std::unique_ptr<ClientRequest>(new ClientRequest);

    ph2q_lock.lock();
    ph2q.push(std::move(req));
    ph2q_cv.notify_one();
    ph2q_lock.unlock();
  }

  {
    std::lock_guard<std::mutex> lock(cr_lock);
    customer_record[laptop.GetCustomerId()] = laptop.GetOrderNumber();
  }
  //committed_index = last_index;
}

void LaptopFactory::CommanderBrocasting(){
  int acceptedNumber = 0;

  // first, establish connections between this commander and all acceptors
  // if the connection has already been created, skip this step
  if (!commander_acceptor_stub_init) {
    for (auto &acceptor : acceptor_map) {
      const ServerAddress &addr = acceptor.second;
      int ret = commander_acceptor_stub[acceptor.first].Init(addr.ip, addr.port);
      
      // if ret==0
      if (!ret) {
        std::cout << "Failed to connect to acceptor" << acceptor.first << std::endl;
        commander_acceptor_stub.erase(acceptor.first);
      } else {
        commander_acceptor_stub[acceptor.first].Identify(3);
      }
    }
    commander_acceptor_stub_init = true;
  }

  for (auto iter = commander_acceptor_stub.begin(); iter != commander_acceptor_stub.end();) {
    PaxosMsg paxosMsg = PaxosMsg(3);
    // LogResponse resp = iter->second.BackupRecord(request);

    // send PaxosMsg to all acceptors
    if (!iter->second.sendPaxosMsg(paxosMsg)) {
      std::cout << "Failed to send PaxosMsg to Acceptor, Acceptor serverId: "<<iter->first<<std::endl;
      iter = commander_acceptor_stub.erase(iter);
      break;
    } else {
      PaxosMsg p2bMsg;
      p2bMsg = iter->second.receivePaxosMsg();
      if (p2bMsg.isAgree()) {
        acceptedNumber ++;
      }
      ++iter;
    }
  }
  std::cout<<"Accepted Number: "<<acceptedNumber<<std::endl;



}

void LaptopFactory::ScoutThread(int id) {
  last_index = -1;
  committed_index = -1;
  //primary_id = -1;
  factory_id = id;
  scout_acceptor_stub_init = false;

  std::unique_lock<std::mutex> ul(erq_lock, std::defer_lock);
  while (true) {
    std::cout<<"scout running"<<std::endl;

    // get a task from queue
    ul.lock();

    if (erq.empty()) {
      erq_cv.wait(ul, [this] { return !erq.empty(); });
    }

    

    auto req = std::move(erq.front());
    erq.pop();

    ul.unlock();
    // task is got from the queue


    // send log request to all the peers
    ScoutBrocasting(req->laptop);
    req->laptop.SetAdminId(id);
    req->prom.set_value(req->laptop);
  }
}

void LaptopFactory::CommanderThread(int id) {
    
    commander_acceptor_stub_init = false;

    std::unique_lock<std::mutex> ul(ph2q_lock, std::defer_lock);
    while (true) {
      ul.lock();
      if (ph2q.empty()) {
        ph2q_cv.wait(ul, [this] { return !ph2q.empty(); });
      }

      auto ph2Req = std::move(ph2q.front());
      ph2q.pop();

      ul.unlock();
      
      CommanderBrocasting();
      // do something with the ph2Req
    }
}

void LaptopFactory::AddAcceptor(int id, std::string ip, int port) {
  ServerAddress addr;
  addr.ip = ip;
  addr.port = port;
  acceptor_map[id] = addr;

  // add the id and server information to serverConfig, which is used to send to client (customer)
  ServerInfo server = ServerInfo(ip, port);
  serverConfig.setServer(id, server);
  std::cout<<"id: "<<id<<std::endl;
  std::cout<<"ip: "<<ip<<std::endl;
  std::cout<<"port: "<<port<<std::endl;

  numOfAcceptors ++;
}
