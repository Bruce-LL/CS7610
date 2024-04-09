#include "ClientThread.h"

#include <iostream>
#include <sstream>

#include "Messages.h"

#include <chrono>
#include <thread>

ClientThreadClass::ClientThreadClass() {}

void ClientThreadClass::Orders() {
  for (int i = 0; i < num_orders; i++) {
    CustomerRequest request;
    LaptopInfo laptop;
    request.SetOrder(customer_id, i, request_type);

    timer.Start();

    // Send order request (which contains only 1 order) to server (engineer) and get reply
    laptop = stub.Order(request);
    timer.EndAndMerge();

    // when !laptop.IsValid(), it means the costmer loses connection with current target server 
    if (!laptop.IsValid()) {
      std::cout << "Invalid laptop, trying to reconnecting..., customer_id: " << customer_id << std::endl;
      

      // try to connect to other servers in serverConfig
      for (const auto& pair : serverConfig.getServers()) {
        std::string ip = pair.second.getIpAdress();
        int port = pair.second.getPortNumber();

        stub = ClientStub();
        std::cout<<"connecting to ip: "<<ip<<", port: "<<port<<", customer_id: "<<customer_id<<std::endl;

        // if failed to connect to this peer, try to connect to next one
        if (!stub.Init(ip, port)) {
          std::cout << "Thread " << customer_id << " failed to connect" << std::endl;
          continue;
        }

        //std::cout << "Reconnected, customer_id: " <<customer_id << std::endl;

        serverConfig = stub.receiveServerConfig();
        serverConfig.print();
        stub.Identify(0);
        goto continue_outer;
      }

      std::cout << "Lost connection to all server nodes, customer_id: " << customer_id << std::endl;
      break;
    }

    continue_outer: ;
  }
}

void ClientThreadClass::Records() {
  CustomerRequest request;
  CustomerRecord record;
  request.SetOrder(customer_id, -1, 2);

  timer.Start();
  record = stub.ReadRecord(request);
  timer.EndAndMerge();

  std::stringstream ss;
  if (!record.IsValid()) {
    ss << "customer " << customer_id << " not exsit" << std::endl;
  } else {
    ss << "customer " << customer_id << " last order "
              << record.GetLastOrder() << std::endl;
  }
  std::cout << ss.str();
}

/**
 * @brief invoked when request_type = 3
 * 
 */
void ClientThreadClass::ScanRecords() {
  for (int i = 0; i < num_orders; i++) {
    CustomerRequest request;
    CustomerRecord record;
    request.SetOrder(i, -1, 2);
    timer.Start();
    record = stub.ReadRecord(request);
    timer.EndAndMerge();
    if (record.IsValid()) {
      std::cout << record.GetCustomerId() << "\t" << record.GetLastOrder()
                << std::endl;
    }
  }
}

void ClientThreadClass::ThreadBody(std::string ip, int port, int id, int orders,
                                   int type) {
  customer_id = id;
  num_orders = orders;
  request_type = type;

  // this will call while ((new_socket = socket.Accept())) in the serverMain
  if (!stub.Init(ip, port)) {
    std::cout << "Thread " << customer_id << " failed to connect" << std::endl;
    return;
  }

  // seems here something is wrong
  serverConfig = stub.receiveServerConfig();
  //std::cout<<"servres size: "<<serverConfig.getServers().size()<<std::endl;
  serverConfig.print();
  //std::cout<<"ipAdd: "<<serverConfig.getServers()[1].getIpAdress()<<std::endl;
  //std::cout<<"port: "<<serverConfig.getServers().at(1).getPortNumber()<<std::endl;

  stub.Identify(0);
  if (request_type == 1) {
    Orders();
  } else if (request_type == 2) {
    Records();
  } else if (request_type == 3) {
    ScanRecords();
  }
}

ClientTimer ClientThreadClass::GetTimer() { return timer; }
