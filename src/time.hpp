#include <chrono>
using namespace std::chrono;

steady_clock::time_point getTime(){
    return steady_clock::now(); 
}
double timeSince(steady_clock::time_point starttime){
    constexpr double ms_conversion=1000.0;
    auto endtime = steady_clock::now();
    return ms_conversion * duration_cast<duration<double>>(endtime - starttime).count();
}
