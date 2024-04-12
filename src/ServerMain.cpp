#include <iostream>
#include <thread>
#include <vector>

#include "ServerSocket.h"
#include "ServerThread.h"
#include "Messages.h"
#include "Utilities.h"

void usage(char *argv[]) {
  std::cout << "Usage: " << argv[0]
            << " [port #] [unique ID] [# peers] (repeat [ID] [IP] [port #])"
            << std::endl;
}

int main(int argc, char *argv[]) {
  int port;
  int engineer_cnt = 0;
  int serverId = -1;
  int num_peers = 0;
  ServerSocket socket;
  LaptopFactory factory;
  std::unique_ptr<ServerSocket> new_socket;
  std::vector<std::thread> thread_vector;


  if (argc < 4) {
    usage(argv);
    return 0;
  }

  port = atoi(argv[1]);
  serverId = atoi(argv[2]); // serverID cannot be larger than 9
  num_peers = atoi(argv[3]);

  
  if (argc < 4 + num_peers * 3) {
    usage(argv);
    return 0;
  }

  if (!socket.Init(port)) {
    std::cout << "Socket initialization failed" << std::endl;
    return 0;
  }

  // add this sever itself as an acceptor
  factory.AddAcceptor(serverId, "127.0.0.1", port);

  // add peers's information as a Acceptors
  for (int i = 0; i < num_peers; i++) {
    int id = atoi(argv[4 + i * 3]);
    std::string ip = argv[5 + i * 3];
    int port = atoi(argv[6 + i * 3]);
    factory.AddAcceptor(id, ip, port);
  }

  // Initialize an Scout thread, runing forever till the end of the program
  // Scout is the first part of a proposer, handling p1a and p1b messages
  thread_vector.push_back(
      std::thread(&LaptopFactory::ScoutThread, &factory, serverId));

  // Initialize an Commander thread, runing forever till the end of the program
  // Commander is the second part of a propser, handling p2a and p2b messages, then send learn messages out
  // thread_vector.push_back(
  //     std::thread(&LaptopFactory::CommanderThread, &factory, serverId));


  // When a new message received by the server, it will create an enginner
  // and let the engineer handle all the messages comming from the same source
  while ((new_socket = socket.Accept())) {
    //std::cout<<"new connection"<<std::endl;
    std::thread engineer_thread(&LaptopFactory::EngineerThread, &factory,
                                std::move(new_socket), engineer_cnt++);
    thread_vector.push_back(std::move(engineer_thread));
  }
  return 0;
}
