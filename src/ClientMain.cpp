#include <array>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "ClientSocket.h"
#include "ClientThread.h"
#include "ClientTimer.h"

int main(int argc, char *argv[]) {
  std::string ip;
  int port;
  int num_customers;
  int num_orders;
  int request_type;
  ClientTimer timer;

  std::vector<std::shared_ptr<ClientThreadClass>> client_vector;
  std::vector<std::thread> thread_vector;

  if (argc < 6) {
    std::cout << "not enough arguments" << std::endl;
    std::cout << argv[0] << "[ip] [port #] [# customers] ";
    std::cout << "[# orders] [request eype 1, 2 or 3]" << std::endl;
    return 0;
  }

  ip = argv[1];
  port = atoi(argv[2]);
  num_customers = atoi(argv[3]);
  num_orders = atoi(argv[4]);
  request_type = atoi(argv[5]);
  if (request_type == 3) {
    num_customers = 1;
  }

  timer.Start();
  for (int i = 0; i < num_customers; i++) {
    // client_cls serves as a customer
    auto client_cls =
        std::shared_ptr<ClientThreadClass>(new ClientThreadClass());
    
    // initialize a thread, this thread will execute the ThreadBody function
    // client_cls is the instance that passed into this thread, serves as a customer
    // here, argument i is the customer id
    std::thread client_thread(&ClientThreadClass::ThreadBody, client_cls, ip,
                              port, i, num_orders, request_type);

    // save all the customers(client_cls) into a vector, in order to fetch their timer at the end of the execution
    client_vector.push_back(std::move(client_cls));
    thread_vector.push_back(std::move(client_thread));
  }
  for (auto &th : thread_vector) {
    th.join();
  }
  timer.End();

  for (auto &cls : client_vector) {
    timer.Merge(cls->GetTimer());
  }
  timer.PrintStats();

  return 1;
}
