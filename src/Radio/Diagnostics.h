#pragma once
#include "../Core/Connector.h"
namespace Radio {
inline std::vector<Telemetry::Controller> Controls(const Core::Snapshot& snapshot){
    std::vector<Telemetry::Controller> result;
    for(auto c:snapshot.controllers){std::string key=c.name;for(char& ch:key)ch=static_cast<char>(tolower(static_cast<unsigned char>(ch)));if(key.find("radio")!=std::string::npos||key.find("vo79")!=std::string::npos||key.find("simplex")!=std::string::npos)result.push_back(c);}
    return result;
}
}
