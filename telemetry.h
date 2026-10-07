#pragma once
#include <cmath>
#include <string>
#include <vector>

namespace Telemetry {
struct Controller { int id; std::string name; float value, minimum, maximum; };
inline std::vector<std::string> SplitNames(const std::string& text) {
    std::vector<std::string> result;
    if(text.empty()) return result;
    size_t start=0;
    while(start<text.size()) {
        auto end=text.find("::",start);
        result.push_back(text.substr(start,end==std::string::npos?end:end-start));
        if(end==std::string::npos) break;
        start=end+2;
    }
    return result;
}
inline bool Valid(float value) { return std::isfinite(value) && value!=-99.0f; }
inline bool SpeedKph(const std::vector<Controller>& controls,float& speed) {
    for(const char* name:{"SpeedometerKPH","SpeedometerMPH"}) {
        for(const auto& c:controls) if(c.name==name && Valid(c.value) && c.value>=0) {
            speed=c.value*(c.name=="SpeedometerMPH"?1.609344f:1.0f);
            return true;
        }
    }
    return false;
}
}
