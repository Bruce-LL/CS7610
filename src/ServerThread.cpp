#include "ServerThread.h"

#include <iostream>
#include <memory>
#include <algorithm>


#include "ServerStub.h"
#include "Utilities.h"

LaptopFactory::LaptopFactory() {

}

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

  laptop = fut.get();  // this thread will wait here, until a laptopInfo is set for prom

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
        if (msg.getProposalNumber() > promisedProposalNumber) {
          promisedProposalNumber = msg.getProposalNumber();
          p1bMsg.setAgree(1);

          // if there is already a accepted value, put it in the p1bMsg
          if (acceptedProposalNumber != -1) {
            std::lock_guard<std::mutex> lock(av_lock);
            p1bMsg.setAcceptedProposal(acceptedProposalNumber);
            p1bMsg.setCommand(acceptedValue); // p1b set command, which is the accepted value
          }
        } else {
          p1bMsg.setAgree(0);
          p1bMsg.setProposalNumber(promisedProposalNumber);
        }
        
        if (!stub.SendPaxosMsg(p1bMsg)) {
          std::cout<<"p1bMsg failed to reach proposer(scout)"<<std::endl;
        }
      } else if (msg.getPhase()==3) { // p2a msg learer
        PaxosMsg p2bMsg = PaxosMsg(4); // p2b msg
        if (msg.getProposalNumber() == promisedProposalNumber) {
          p2bMsg.setAgree(1);
          acceptedProposalNumber = msg.getProposalNumber();
          acceptedValue = msg.getCommand();
        } else {
          p2bMsg.setAgree(0);
        }

        // send the p2b message back to proposer (scout)
        if (!stub.SendPaxosMsg(p2bMsg)) {
          std::cout<<"p2bMsg failed to reach proposer(scout)"<<std::endl;
        }
      } else {
        std::cout<<"unknown phase";
      }
    }
  } else if (identity==3){ // learner, contacting with Commander
    std::cout<<"Learner-Commander connection Initilizd";
    while (true) {
      PaxosMsg msg = stub.ReceivePaxosMsg(); // should be a learn message
      if (msg.getPhase()<0) { //this will happen when commander's process shut down
        std::cout << "Lost Contact with proposer(commander)" << std::endl;
        break;
      }

      if (msg.getPhase()==5) {
        Learn(msg.getCommand());
      } else {
        std::cout << "Learner Message invalid, phase = "<<msg.getPhase()<< std::endl;
      }
    }
  } else {
    std::cout << "Undefined identity: " << identity << std::endl;
  }
}

void LaptopFactory::Learn(Command cmd) {
  std::lock_guard<std::mutex> lock(decisionMap_lock);

  decisionMap[cmd.getSlot()] = cmd;

  // if slot_out in decisionMap keyset
  while (decisionMap.find(slot_out) != decisionMap.end()){
    // TODO: commit the decision, now just print it out, what else can we do?
    
    decisionMap[slot_out].print();
    slot_out ++;

    // store it into local with factory_id as file name
    std::string filename = std::to_string(factory_id) + " FactoryLog.txt"; // Construct filename
    saveCommandToFile(slot_out, cmd, filename);
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

  // establish connections to all peers (including itself)
  if (!scout_acceptor_stub_init) {
    for (auto &acceptor : acceptor_map) {
      const ServerAddress &addr = acceptor.second;
      int ret = scout_acceptor_stub[acceptor.first].Init(addr.ip, addr.port);
      
      // if ret==0
      if (!ret) {
        std::cout << "Failed to connect to Acceptor " << acceptor.first << std::endl;
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
  
  start_proposalNum_compete:
  
  // Now start competing a proposal Number in the whole system
  int promisedNum = 0; // number of promise collected from acceptors
  int highestProposlNum = -1; // highest (promised) proposal number collected from acceptors

  Command cmd; // store the accepted value comming from acceptors
  int acceptor_acceptedProposal = -1;
  // compete for a highest proposal number
  // track p1a and p1b messages
  while (true) {
    // definition of acceptor_stub:   std::map<int, ClientStub> acceptor_stub;
    for (auto iter = scout_acceptor_stub.begin(); iter != scout_acceptor_stub.end();) {
      PaxosMsg paxosMsg = PaxosMsg(1);
      paxosMsg.setProposalNumber(proposalNumber);
      // send PaxosMsg to all acceptors
      if (!iter->second.sendPaxosMsg(paxosMsg)) {
        std::cout << "Failed to send PaxosMsg to Acceptor, Acceptor serverId: "<<iter->first<<std::endl;
      } else {
        PaxosMsg p1bMsg;
        p1bMsg = iter->second.receivePaxosMsg();
        highestProposlNum = std::max(highestProposlNum, p1bMsg.getProposalNumber());
              
        if (p1bMsg.isAgree()) {
          promisedNum ++;
          if (p1bMsg.getAcceptedProposal() > acceptor_acceptedProposal) {
            acceptor_acceptedProposal = p1bMsg.getAcceptedProposal();
            cmd = p1bMsg.getCommand();
          }
        } else {
          // do nothing
        }
      }
      ++iter;
    }
    
    if (promisedNum <= numOfAcceptors/2) { 
      // if got rejected, use a larger proposalNumber to complete again
      proposalNumber = highestProposlNum + 1;
      continue;
    } else {  // majority of acceptors primised
      break;
    }
  }
  // by now, we got a proposalNumber which is the highest in the system at the moment

  // To propose its own cmd (proposal) or a cmd (proposal) received from the system
  // myMap.find(keyToCheck) != myMap.end() means the keyToCheck IS in the key set of map
  if (acceptor_acceptedProposal == -1 || (decisionMap.find(cmd.getSlot()) !=  decisionMap.end())) {
    // propose own value:
    // below gives enough message for a command
    // except client ip address is set to default '000.000.000.000'
    Command selfCommand = Command(laptop);
    {
      std::lock_guard<std::mutex> lock(slot_in_lock);
      // if slot_in is in decisionMap's keyset
      while (decisionMap.find(slot_in) != decisionMap.end()) {
        slot_in ++;
      }
      selfCommand.setSlot(slot_in);
    }
    selfCommand.setCommandId(generateCommandID(factory_id));
   
    // argument 1 means using self command
    int res = AcceptPhaseBrocasting(selfCommand);

    if (res == 1) { // accept
       CommanderBrocasting(selfCommand);
    } else if (res == 0) { // full reject
      // restart prepare phase
      // the req is kept
      goto start_proposalNum_compete;
    } else if (res == -1) { // part reject
      // we don't need to to anything here
      // it will  go to end the ScoutBrocasting() function
      // our command will be proposed by peers
    } else {
      std::cout<<"undefined AcceptPhaseBrocasting() result"<<std::endl;
    }
  
  } else { // propose cmd from the system
    int res = AcceptPhaseBrocasting(cmd);
    if (res == 1) { // approved, collect majority's agree
      CommanderBrocasting(cmd);
    } else { // full reject or part reject
      // drop the cmd and restart prepare phase
      goto start_proposalNum_compete;
    }
  }

  // 
  // std::unique_ptr<ClientRequest> req =
  //       std::unique_ptr<ClientRequest>(new ClientRequest);

  // ph2q_lock.lock();
  // ph2q.push(std::move(req));
  // ph2q_cv.notify_one();
  // ph2q_lock.unlock();
  {
    std::lock_guard<std::mutex> lock(cr_lock);
    customer_record[laptop.GetCustomerId()] = laptop.GetOrderNumber();
  }
}

/**
 * @brief send p2a message to acceptors and and collect p2b messesages from acceptors
 * 
 * 
 *                     
 * @param cmd the command (either from local server or other servers)
 * @return int - 1 -  accept
 *             - 0 - full reject
 *             - -1 - part reject
 */
int LaptopFactory::AcceptPhaseBrocasting(Command cmd) {
  // don't need to establish connection to server's engineer
  // the connections are establish by ScoutBrocasting() and stored in scout_acceptor_stub
  int accept_number = 0;

  for (auto iter = scout_acceptor_stub.begin(); iter != scout_acceptor_stub.end();) {
    PaxosMsg p2aMsg = PaxosMsg(3);
    p2aMsg.setCommand(cmd);
    p2aMsg.setProposalNumber(proposalNumber);
    // send PaxosMsg to all acceptors
    if (!iter->second.sendPaxosMsg(p2aMsg)) {
      std::cout << "Failed to send p2aMsg to Acceptor, Acceptor serverId: "<<iter->first<<std::endl;
    } else {
      PaxosMsg p2bMsg;
      p2bMsg = iter->second.receivePaxosMsg();

      if (p2bMsg.isAgree()) {
        accept_number ++;
      } else {
        // do nothing
      }
    }
    ++iter;
  }
  
  if (accept_number == 0) { // full reject
    return 0;
  } else if (accept_number <= numOfAcceptors / 2) { // part reject
    return -1;
  } else {  // accepted by majority
    return 1;
  }
}


/**
 * @brief send LEARN messages to all the leaners
 * 
 */
void LaptopFactory::CommanderBrocasting(Command cmd) {

  int acceptedNumber = 0;

  // first, establish connections between this commander and all learners (Engineer)
  // if the connection has already been created, skip this step
  if (!commander_learner_stub_init) {
    for (auto &learner : acceptor_map) {
      const ServerAddress &addr = learner.second;
      int ret = commander_learner_stub[learner.first].Init(addr.ip, addr.port);
      
      // if ret==0
      if (!ret) {
        std::cout << "Failed to connect to Learner" << learner.first << std::endl;
        commander_learner_stub.erase(learner.first);
      } else {
        commander_learner_stub[learner.first].Identify(3);
      }
    }
    commander_learner_stub_init = true;
  }
  
  for (auto iter = commander_learner_stub.begin(); iter != commander_learner_stub.end();) {
    PaxosMsg paxosMsg = PaxosMsg(5);
    paxosMsg.setCommand(cmd);
    // send PaxosMsg (learn message) to all acceptors
    if (!iter->second.sendPaxosMsg(paxosMsg)) { // send failed
      std::cout << "Failed to send PaxosMsg to Learner "<<iter->first<<std::endl;
    }
    ++iter;
  }
}

void LaptopFactory::ScoutThread(int id) {
  last_index = -1;
  committed_index = -1;
  //primary_id = -1;
  factory_id = id;
  scout_acceptor_stub_init = false;
  commander_learner_stub_init = false;  // will be set to true in AcceptBrocasting() function

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


    // broadcasting the paxos message
    // send p1a, receive p1b, send p2a, receive p2b
    ScoutBrocasting(req->laptop);
    
    req->laptop.SetAdminId(id);
    req->prom.set_value(req->laptop);
  }
}

// void LaptopFactory::CommanderThread(int id) {
    
//     commander_acceptor_stub_init = false;

//     std::unique_lock<std::mutex> ul(ph2q_lock, std::defer_lock);
//     while (true) {
//       ul.lock();
//       if (ph2q.empty()) {
//         ph2q_cv.wait(ul, [this] { return !ph2q.empty(); });
//       }

//       auto ph2Req = std::move(ph2q.front());
//       ph2q.pop();

//       ul.unlock();
      
//       CommanderBrocasting();
//       // do something with the ph2Req
//     }
// }

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
