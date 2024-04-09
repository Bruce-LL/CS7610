#ifndef __CLIENT_STUB_H__
#define __CLIENT_STUB_H__

#include <string>

#include "ClientSocket.h"
#include "Messages.h"

class ClientStub {
private:
  

public:
  ClientSocket socket;
  ClientStub();
  int Init(std::string ip, int port);
  int Reconnect(std::string ip, int port);

  LaptopInfo Order(CustomerRequest order);
  CustomerRecord ReadRecord(CustomerRequest order);
  LogResponse BackupRecord(LogRequest log);
  ServerConfig receiveServerConfig();
  void Identify(int role);
};

#endif // end of #ifndef __CLIENT_STUB_H__
