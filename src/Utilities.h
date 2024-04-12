#ifndef __UTILITISE_H__
#define __UTILITISE_H__

#include <string>


int generateCommandID(int serverID);

void saveMapToFile(std::map<int, Command>& decisionMap, const std::string& filename);
std::map<int, Command> loadMapFromFile(const std::string& filename);
void saveCommandToFile(int slot, const Command& command, const std::string& filename);


#endif // end of #ifndef __UTILITISE_H__