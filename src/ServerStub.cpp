#include "ServerStub.h"

#include <arpa/inet.h>

#include <iostream>

#include <vector>

ServerStub::ServerStub() {}

void ServerStub::Init(std::unique_ptr<ServerSocket> socket) {
  this->socket = std::move(socket);
}

CustomerRequest ServerStub::ReceiveRequest() {
  char buffer[32];
  CustomerRequest request;
  if (socket->Recv(buffer, request.Size(), 0)) {
    request.Unmarshal(buffer);
  }
  return request;
}

int ServerStub::SendLaptop(LaptopInfo info) {
  char buffer[32];
  info.Marshal(buffer);
  return socket->Send(buffer, info.Size(), 0);
}

int ServerStub::SendServerConfig(ServerConfig serverConfig) {
  // send the size first
  size_t dataSize = serverConfig.size();
  uint32_t netDataSize = htonl(static_cast<uint32_t>(dataSize));
  socket->Send(reinterpret_cast<char*>(&netDataSize), sizeof(netDataSize), 0);
  
  // now send the marshalled servreConfig data
  std::vector<char> buffer(serverConfig.size());
  serverConfig.Marshal(buffer.data());
  return socket->Send(buffer.data(), buffer.size(), 0);
}

int ServerStub::ReturnRecord(CustomerRecord record) {
  char buffer[32];
  record.Marshal(buffer);
  return socket->Send(buffer, record.Size(), 0);
}

int ServerStub::ReceiveIndentity() {
  char buffer[32];
  int identity = -1;
  if (socket->Recv(buffer, sizeof(int), 0)) {
    identity = ntohl(*(int *)buffer);
  }
  return identity;
}

LogRequest ServerStub::ReceiveLogRequest() {
  char buffer[32];
  LogRequest response;
  if (socket->Recv(buffer, response.Size(), 0)) {
    response.Unmarshal(buffer);
  }
  return response;
}

int ServerStub::ReturnLogResponse(LogResponse response) {
  char buffer[32];
  response.Marshal(buffer);
  return socket->Send(buffer, response.Size(), 0);
}

PaxosMsg ServerStub::ReceivePaxosMsg() {
  PaxosMsg receivedMsg;
  char buffer[64]; // A PaxosMsg should not exceed 51 bytes. 64 bytes should be save for it
  if (socket->Recv(buffer, receivedMsg.size(), 0)) {
    receivedMsg.Unmarshal(buffer);
  }
  return receivedMsg;
}

int ServerStub::SendPaxosMsg(PaxosMsg msg) {
  char buffer[64]; // A PaxosMsg should not exceed 51 bytes. 64 bytes should be save for it
  msg.Marshal(buffer);
  return socket->Send(buffer, msg.size(), 0);
}

