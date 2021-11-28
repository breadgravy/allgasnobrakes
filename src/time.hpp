#pragma once

#include <chrono>
using namespace std::chrono;

steady_clock::time_point getTime(){
    return steady_clock::now(); 
}
double timeSinceMilli(steady_clock::time_point starttime){
    constexpr double ms_conversion=1e3;
    auto endtime = steady_clock::now();
    return ms_conversion * duration_cast<duration<double>>(endtime - starttime).count();
}
double timeSinceMicro(steady_clock::time_point starttime){
    constexpr double ms_conversion=1e6;
    auto endtime = steady_clock::now();
    return ms_conversion * duration_cast<duration<double>>(endtime - starttime).count();
}
