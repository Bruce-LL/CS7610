#include "ServerStub.h"

#include <arpa/inet.h>

#include <iostream>

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
