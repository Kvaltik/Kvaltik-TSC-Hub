#include "../telemetry.h"
#include "../nav_route.h"
#include <iostream>
#include <limits>
int main() {
    int failures=0;
    auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)++failures;};
    check(Telemetry::SplitNames("").empty(),"empty controllers");
    auto names=Telemetry::SplitNames("Throttle::::Brake::");
    check(names.size()==3 && names[1].empty() && names[2]=="Brake","preserve controller IDs and trailing separator");
    float speed=0;
    check(!Telemetry::SpeedKph({},speed),"missing speed is unavailable");
    check(Telemetry::SpeedKph({{0,"SpeedometerMPH",60,0,100}},speed) && std::abs(speed-96.56064f)<0.001f,"mph conversion");
    check(Telemetry::SpeedKph({{0,"SpeedometerMPH",60,0,100},{1,"SpeedometerKPH",80,0,160}},speed) && speed==80,"prefer explicit km/h");
    check(!Telemetry::SpeedKph({{0,"SpeedometerKPH",-99,0,160}},speed),"unavailable speed sentinel");
    check(!Telemetry::SpeedKph({{0,"SpeedometerKPH",std::numeric_limits<float>::quiet_NaN(),0,160}},speed),"reject NaN");
    check(Telemetry::SpeedKph({{0,"SpeedometerKPH",0,0,160}},speed) && speed==0,"stationary is valid");
    check(!Telemetry::SpeedKph({{0,"Speed",40,0,100}},speed),"unknown units not guessed");
    std::istringstream input("\xEF\xBB\xBFname;latitude;longitude\r\nPraha;50.08;14.43\r\nBrno;49.19;16.61\r\n");
    auto stops=Navigation::Parse(input);
    check(stops.size()==2 && stops[1].name=="Brno","UTF-8 BOM and CRLF route");
    check(Navigation::DistanceKm(50.08,14.43,stops[0])<0.001,"distance at stop");
    check(std::abs(Navigation::DistanceKm(0,0,{"test",0,1})-111.195)<0.01,"great-circle distance");
    for(const auto& text:{"", "A;91;0", "A;0;181", "A;nan;0", "A;2x;0", ";0;0", "A;1", "A;1;2;3"}) {
        bool rejected=false;try {std::istringstream invalid(text);Navigation::Parse(invalid);}catch(const std::exception&){rejected=true;}
        check(rejected,"reject invalid route");
    }
    return failures?1:0;
}
