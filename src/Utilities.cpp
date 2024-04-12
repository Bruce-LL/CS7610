#include <iostream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#include <chrono>
#include <sstream>

int generateCommandID(int serverID) {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
    // Get the duration value in milliseconds
    long long milliseconds = value.count();

    // Convert milliseconds to string
    std::ostringstream oss;
    oss << milliseconds;
    std::string milliseconds_str = oss.str();

    if (milliseconds_str.length() > 8) {
        milliseconds_str = milliseconds_str.substr(milliseconds_str.length() - 8);
    }

    return (10 + serverID) * 10000000 + std::stoi(milliseconds_str);
}