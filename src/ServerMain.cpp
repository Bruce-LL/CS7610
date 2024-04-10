#include <iostream>
#include <thread>
#include <vector>

#include "ServerSocket.h"
#include "ServerThread.h"
#include "Messages.h"

void usage(char *argv[]) {
  std::cout << "Usage: " << argv[0]
            << " [port #] [unique ID] [# peers] (repeat [ID] [IP] [port #])"
            << std::endl;
}

int main(int argc, char *argv[]) {
  int port;
  int engineer_cnt = 0;
  int admin_id = -1;
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
  admin_id = atoi(argv[2]);
  num_peers = atoi(argv[3]);

  if (argc < 4 + num_peers * 3) {
    usage(argv);
    return 0;
  }

  if (!socket.Init(port)) {
    std::cout << "Socket initialization failed" << std::endl;
    return 0;
  }
  

  for (int i = 0; i < num_peers; i++) {
    int id = atoi(argv[4 + i * 3]);
    std::string ip = argv[5 + i * 3];
    int port = atoi(argv[6 + i * 3]);
    factory.AddAdmin(id, ip, port);
  }

  // Initialize an admin thread, runing forever till the end of the program
  // TODO: modify the AdminThread to ScoutThread
  thread_vector.push_back(
      std::thread(&LaptopFactory::AdminThread, &factory, admin_id));

  // TODO: add and initialize a CommanderThread
  

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
