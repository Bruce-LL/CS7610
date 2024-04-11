#include <iostream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

std::string getLocalIp() {
    struct ifaddrs *ifAddrStruct = nullptr, *ifa = nullptr;
    char addressBuffer[INET_ADDRSTRLEN] = {0};

    if (getifaddrs(&ifAddrStruct) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) { // Check for IPv4
            // Avoid loopback addresses
            const struct sockaddr_in *addr = (const struct sockaddr_in *)ifa->ifa_addr;
            if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                continue;
            }
            inet_ntop(AF_INET, &addr->sin_addr, addressBuffer, sizeof(addressBuffer));
            freeifaddrs(ifAddrStruct); // Clean up memory
            return std::string(addressBuffer); // Return the first non-loopback IPv4 address
        }
    }

    freeifaddrs(ifAddrStruct); // Clean up memory
    return ""; // Return empty string if no non-loopback address was found
}