#include "ClientStub.h"

#include <arpa/inet.h>
#include <iostream>

ClientStub::ClientStub() {}

int ClientStub::Init(std::string ip, int port) { return socket.Init(ip, port); }

int ClientStub::Reconnect(std::string ip, int port) {
  socket.reset();
  return socket.Init(ip, port);
}

LaptopInfo ClientStub::Order(CustomerRequest order) {
  LaptopInfo info;
  char buffer[32];
  int size;
  order.Marshal(buffer);
  size = order.Size();
  if (socket.Send(buffer, size, 0)) {
    size = info.Size();
    if (socket.Recv(buffer, size, 0)) {
      info.Unmarshal(buffer);
    }
  }
  
  return info;
}

CustomerRecord ClientStub::ReadRecord(CustomerRequest order) {
  CustomerRecord record;
  char buffer[32];
  int size;
  order.Marshal(buffer);
  size = order.Size();
  if (socket.Send(buffer, size, 0)) {
    size = record.Size();
    if (socket.Recv(buffer, size, 0)) {
      record.Unmarshal(buffer);
    }
  }
  return record;
}

LogResponse ClientStub::BackupRecord(LogRequest log) {
  LogResponse resp;
  char buffer[32];
  int size;
  log.Marshal(buffer);
  size = log.Size();
  if (socket.Send(buffer, size, 0)) {
    size = resp.Size();
    if (socket.Recv(buffer, size, 0)) {
      resp.Unmarshal(buffer);
    }
  }
  return resp;
}

ServerConfig ClientStub::receiveServerConfig() {
  char header[sizeof(uint32_t)]; // Assuming the header just contains the size.
  if (socket.Recv(header, sizeof(uint32_t), 0) <= 0) {
    std::cout<<"failed to receive header"<<std::endl;
  } // Receive the header first.
   
  uint32_t messageSize;
  memcpy(&messageSize, header, sizeof(uint32_t));
  messageSize = ntohl(messageSize); // Convert from network byte order if needed.  

  ServerConfig serverConfig;
  std::vector<char> buffer(messageSize); 
  if (socket.Recv(buffer.data(), messageSize, 0)) {
    serverConfig.Unmarshal(buffer.data());
  } else {
    std::cout<<"failed to receive serverConfig..."<<"size: "<<messageSize<<std::endl;
  }
  //std::cout<<"servres size: "<<serverConfig.getServers().size()<<std::endl;
  //serverConfig.print();
  
  return serverConfig;
}

void ClientStub::Identify(int role) {
  int net_role = htonl(role);
  socket.Send((char *)&net_role, sizeof(net_role), 0);
}


int ClientStub::sendPaxosMsg(PaxosMsg msg) {
  char buffer[64]; // a marshalled PaxosMsg should not exceed 51 bytes, so 64 bytes is safe
  msg.Marshal(buffer);
  return socket.Send(buffer, msg.size(), 0);
}

PaxosMsg ClientStub::receivePaxosMsg() {
  PaxosMsg receivedMsg;
  char buffer[64]; // A PaxosMsg should not exceed 51 bytes. 64 bytes should be save for it
  if (socket.Recv(buffer, receivedMsg.size(), 0)) {
    receivedMsg.Unmarshal(buffer);
  }
  return receivedMsg;
}
