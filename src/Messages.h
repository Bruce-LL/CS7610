#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include <string>
#include <iostream>
#include <map>
#include <string>

/**
 * @brief Customer request sent by a customer to a engineer in a server
 *        One request contains the information of one order
 * 
 */
class CustomerRequest {
private:
  int customer_id;
  int order_number;  // it is the orderID
  int request_type;

public:
  CustomerRequest();
  void operator=(const CustomerRequest &order) {
    customer_id = order.customer_id;
    order_number = order.order_number;
    request_type = order.request_type;
  }
  void SetOrder(int cid, int order_num, int type);
  int GetCustomerId();
  int GetOrderNumber();
  int GetRequestType();

  int Size();

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid();

  void Print();
};

/**
 * @brief 
 * 
 */
class CustomerRecord {
private:
  int customer_id;
  int last_order;

public:
  CustomerRecord();
  void operator=(const CustomerRecord &record) {
    customer_id = record.customer_id;
    last_order = record.last_order;
  }

  void SetCustomerRecord(int cid, int order_number);
  int GetCustomerId();
  int GetLastOrder();

  int Size();

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid();

  void Print();
};

/**
 * @brief 
 * 
 */
class LaptopInfo {
private:
  int customer_id;
  int order_number;
  int request_type;
  int engineer_id;
  int admin_id;

public:
  LaptopInfo();
  void operator=(const LaptopInfo &info) {
    customer_id = info.customer_id;
    order_number = info.order_number;
    request_type = info.request_type;
    engineer_id = info.engineer_id;
    admin_id = info.admin_id;
  }
  void SetInfo(int cid, int order_num, int type, int engid, int admid);
  void CopyOrder(CustomerRequest order);
  void SetEngineerId(int id);
  void SetAdminId(int id);

  int GetCustomerId();
  int GetOrderNumber();
  int GetRequestType();
  int GetEngineerId();
  int GetAdminId();

  int Size();

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid();

  void Print();
};


/**
 * @brief 
 * 
 */
class MapOp {
private:
  int opcode;
  int arg1;
  int arg2;

public:
  MapOp();
  void operator=(const MapOp &op) {
    opcode = op.opcode;
    arg1 = op.arg1;
    arg2 = op.arg2;
  }

  void SetMapOp(int op, int a1, int a2);
  void CopyMapOp(MapOp op);
  void SetOpCode(int op) { opcode = op; }
  void SetArg1(int a1) { arg1 = a1; }
  void SetArg2(int a2) { arg2 = a2; }

  int GetOpCode() { return opcode; }
  int GetArg1() { return arg1; }
  int GetArg2() { return arg2; }

  int Size();

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid() { return (opcode != -1); }

  void Print();
};

/**
 * @brief 
 * 
 */
class LogRequest {
private:
  int factory_id;
  int committed_index;
  int last_index;
  MapOp op;

public:
  LogRequest();
  void operator=(const LogRequest &request) {
    factory_id = request.factory_id;
    committed_index = request.committed_index;
    last_index = request.last_index;
    op = request.op;
  }
  void CopyRequest(LogRequest request);
  void SetFactoryId(int id) { factory_id = id; }
  void SetCommittedIndex(int index) { committed_index = index; }
  void SetLastIndex(int index) { last_index = index; }
  void SetMapOp(MapOp op) { this->op = op; }

  int GetFactoryId() { return factory_id; }
  int GetCommittedIndex() { return committed_index; }
  int GetLastIndex() { return last_index; }
  MapOp GetMapOp() { return op; }

  int Size();

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid() { return (factory_id != -1); }

  void Print();
};


/**
 * @brief 
 * 
 */
class LogResponse {
private:
  int factory_id;
public:
  LogResponse();
  void operator=(const LogResponse &response) {
    factory_id = response.factory_id;
  }
  void SetFactoryId(int id) { factory_id = id; }
  int GetFactoryId() { return factory_id; }

  int Size() { return sizeof(factory_id); }

  void Marshal(char *buffer);
  void Unmarshal(char *buffer);

  bool IsValid() { return (factory_id != -1); }

  void Print();
};


/**
 * @brief Wrap the serverIP and server port number in a class
 * 
 */
class ServerInfo {
  private:
    std::string ipAddress;
    int portNumber;
  
  public:
    ServerInfo() {};
    ServerInfo(std::string ipAddress, int portNumber);
    std::string getIpAdress() const { return ipAddress; }
    int getPortNumber() const { return portNumber; }
    size_t size() const;
    void Marshal(char *buffer) const;
    void Unmarshal(char *buffer);
};


/**
 * @brief The message contains all the servers' information in the server system
 *        This message will be sent by an engineer when connection is first established
 * 
 */
class ServerConfig {
  private:
    std::map<int, ServerInfo> servers;
  
  public:
    ServerConfig();
    std::map<int, ServerInfo> getServers();
    void setServer(int serverId, ServerInfo Info);
    size_t size() const;
    void Marshal(char *buffer);
    void Unmarshal(char *buffer);

    void print();
};




#endif // #ifndef __MESSAGES_H__
