#pragma once
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace Navigation {
struct Stop { std::string name; double latitude,longitude; };
inline double Coordinate(const std::string& value,double limit) {
    size_t used=0;double number=std::stod(value,&used);
    if(used!=value.size() || !std::isfinite(number) || std::abs(number)>limit) throw std::runtime_error("Invalid coordinate");
    return number;
}
inline std::vector<Stop> Parse(std::istream& input) {
    std::vector<Stop> result;std::string line;size_t row=0;
    while(std::getline(input,line)) {
        ++row;if(!line.empty() && line.back()=='\r')line.pop_back();
        if(row==1 && line.compare(0,3,"\xEF\xBB\xBF")==0)line.erase(0,3);
        if(line.empty())continue;
        if(row==1 && line=="name;latitude;longitude")continue;
        auto a=line.find(';'),b=a==std::string::npos?a:line.find(';',a+1);
        if(a==0 || a==std::string::npos || b==std::string::npos || line.find(';',b+1)!=std::string::npos) throw std::runtime_error("Invalid route row "+std::to_string(row));
        result.push_back({line.substr(0,a),Coordinate(line.substr(a+1,b-a-1),90),Coordinate(line.substr(b+1),180)});
        if(result.size()>10000)throw std::runtime_error("Too many stops");
    }
    if(result.empty())throw std::runtime_error("Empty route");
    return result;
}
inline double DistanceKm(double lat,double lon,const Stop& stop) {
    constexpr double rad=3.14159265358979323846/180;
    double dlat=(stop.latitude-lat)*rad,dlon=(stop.longitude-lon)*rad;
    double a=std::sin(dlat/2)*std::sin(dlat/2)+std::cos(lat*rad)*std::cos(stop.latitude*rad)*std::sin(dlon/2)*std::sin(dlon/2);
    if(a>1)a=1;if(a<0)a=0;
    return 6371.0088*2*std::atan2(std::sqrt(a),std::sqrt(1-a));
}
}
