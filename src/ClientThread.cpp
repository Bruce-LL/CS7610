#include "ClientThread.h"

#include <iostream>
#include <sstream>

#include "Messages.h"

ClientThreadClass::ClientThreadClass() {}

void ClientThreadClass::Orders() {
  for (int i = 0; i < num_orders; i++) {
    CustomerRequest request;
    LaptopInfo laptop;
    request.SetOrder(customer_id, i, request_type);

    timer.Start();
    laptop = stub.Order(request);
    timer.EndAndMerge();

    if (!laptop.IsValid()) {
      std::cout << "Invalid laptop " << customer_id << std::endl;
      break;
    }
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
  if (!stub.Init(ip, port)) {
    std::cout << "Thread " << customer_id << " failed to connect" << std::endl;
    return;
  }

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
