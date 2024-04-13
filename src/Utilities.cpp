#include <iostream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#include <chrono>
#include <sstream>
#include "Messages.h"
#include "Utilities.h"
#include <fstream>
#include <map>

int generateCommandID(int serverID) {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::microseconds>(epoch);
    // Get the duration value in microseconds
    long long microseconds = value.count();

    // Convert milliseconds to string
    std::ostringstream oss;
    oss << microseconds;
    std::string microseconds_str = oss.str();

    if (microseconds_str.length() > 8) {
        microseconds_str = microseconds_str.substr(microseconds_str.length() - 8);
    }
    
    return (10 + serverID) * 10000000 + std::stoi(microseconds_str);
}


// Function to save the map to a file
void saveMapToFile(std::map<int, const Command>& decisionMap, const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& pair : decisionMap) {
            file << pair.first << " " << pair.second.getSlot() << " " << pair.second.getCommandId() << " " << pair.second.getClientIp() << " "
                 << pair.second.getCustomerId() << " " << pair.second.getOrderId() << std::endl;
        }
        file.close();
    } else {
        std::cerr << "Unable to open file " << filename << " for writing." << std::endl;
    }
}

// Function to load the map from a file for recovery
std::map<int, Command> loadMapFromFile(const std::string& filename) {
    std::map<int, Command> decisionMap;
    std::ifstream file(filename);
    if (file.is_open()) {
        int key, slot, commandId, customerId, orderId;
        std::string clientIp;
        while (file >> key >> slot >> commandId >> clientIp >> customerId >> orderId) {
            decisionMap[key] = Command(slot, commandId, clientIp, customerId, orderId);
        }
        file.close();
    } else {
        std::cerr << "Unable to open file " << filename << " for reading." << std::endl;
    }
    return decisionMap;
}

void saveCommandToFile(int slot, const Command& command, const std::string& filename) {
    std::ofstream file(filename, std::ios::app); // Open file in append mode
    if (file.is_open()) {
        file << slot << " " << command.getSlot() << " " << command.getCommandId() << " " << command.getClientIp() << " "
             << command.getCustomerId() << " " << command.getOrderId() << std::endl;
        file.close();
    } else {
        std::cerr << "Unable to open file " << filename << " for writing." << std::endl;
    }
}