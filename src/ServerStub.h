#ifndef __SERVER_STUB_H__
#define __SERVER_STUB_H__

#include <memory>

#include "Messages.h"
#include "ServerSocket.h"

class ServerStub {
private:
  std::unique_ptr<ServerSocket> socket;

public:
  ServerStub();
  void Init(std::unique_ptr<ServerSocket> socket);
  CustomerRequest ReceiveRequest();
  LogRequest ReceiveLogRequest();
  int ReturnLogResponse(LogResponse response);
  int SendLaptop(LaptopInfo info);
  int ReturnRecord(CustomerRecord record);
  int ReceiveIndentity();
  int SendServerConfig(ServerConfig serverConfig); // send from engineer to customer
  PaxosMsg ReceivePaxosMsg();
  int SendPaxosMsg(PaxosMsg msg); 
};

#endif // end of #ifndef __SERVER_STUB_H__
