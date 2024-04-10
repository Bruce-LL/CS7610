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

  std::promise<LaptopInfo> prom;
  std::future<LaptopInfo> fut = prom.get_future();

  std::unique_ptr<AdminRequest> req =
      std::unique_ptr<AdminRequest>(new AdminRequest);
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

  //std::this_thread::sleep_for(std::chrono::milliseconds(500));

  stub.SendServerConfig(serverConfig);

  // before sending a message, the sender sends an int identity to the engineer
  // identity = 0 means the this engineer is talking to a client (customer)
  //              in this case, the message received is a CustomerRequest type
  // identity = 1 means the this engineer is talking to a server
  //              in this case, the message received is a LogRequest type
  int identity = stub.ReceiveIndentity();
  if (identity == 0) {
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
  } else if (identity==2) { // Acceptor
    std::cout<<"Acceptor Initilizd";
    while (true) {
      // receive PaxosMsg
      // if 
      
      // p1a (prepareMsg) receive
      // p1b (promise or reject) send out

      // p2a (acceptMsg) receive (only when giving )
      // p2b ()
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

void LaptopFactory::PFA(LaptopInfo &laptop) {
  // if (primary_id != factory_id) {
  //   primary_id = factory_id;
  // }

  if (!admin_stub_init) {
    for (auto &admin : admin_map) {
      const ServerAddress &addr = admin.second;
      int ret = admin_stub[admin.first].Init(addr.ip, addr.port);
      
      // if ret==0
      if (!ret) {
        std::cout << "Failed to connect to admin (peer) " << admin.first << std::endl;
        admin_stub.erase(admin.first);
      } else {
        admin_stub[admin.first].Identify(1);
      }
    }
    admin_stub_init = true;
  }

  MapOp op;
  op.SetMapOp(1, laptop.GetCustomerId(), laptop.GetOrderNumber());
  smr_log.emplace_back(op);
  last_index = smr_log.size() - 1;

  // below is how the primary node sends LogRequest to all the peers.
  // we can whipe it for now

  LogRequest request = CreateLogRequest(op);

  // definition of admin_stub:   std::map<int, ClientStub> admin_stub;
  for (auto iter = admin_stub.begin(); iter != admin_stub.end();) {
    std::cout<<"hh"<<std::endl;
    LogResponse resp = iter->second.BackupRecord(request);
    if (!resp.IsValid()) {

      std::cout << "Failed to backup record to admin"<<std::endl;
      iter = admin_stub.erase(iter);
      break;
    } else {
      ++iter;
    }
  }
  {
    std::lock_guard<std::mutex> lock(cr_lock);
    customer_record[laptop.GetCustomerId()] = laptop.GetOrderNumber();
  }
  committed_index = last_index;
}

void LaptopFactory::AdminThread(int id) {
  last_index = -1;
  committed_index = -1;
  //primary_id = -1;
  factory_id = id;
  admin_stub_init = false;

  std::unique_lock<std::mutex> ul(erq_lock, std::defer_lock);
  while (true) {
    ul.lock();

    if (erq.empty()) {
      erq_cv.wait(ul, [this] { return !erq.empty(); });
    }

    auto req = std::move(erq.front());
    erq.pop();

    ul.unlock();

    // send log request to all the peers
    PFA(req->laptop);
    req->laptop.SetAdminId(id);
    req->prom.set_value(req->laptop);
  }
}

void LaptopFactory::AddAdmin(int id, std::string ip, int port) {
  ServerAddress addr;
  addr.ip = ip;
  addr.port = port;
  admin_map[id] = addr;

  // add the id and server information to serverConfig, which is used to send to client (customer)
  ServerInfo server = ServerInfo(ip, port);
  serverConfig.setServer(id, server);
  std::cout<<"id: "<<id<<std::endl;
  std::cout<<"ip: "<<ip<<std::endl;
  std::cout<<"port: "<<port<<std::endl;
}
