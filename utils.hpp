#pragma once

#include "defs.hpp"

#include <chrono>
#include <thread>
#include <utility>

class StopWatch
{
public:
    constexpr StopWatch() : _T0() {}
    inline double now() const;
    inline void start();
    decltype(std::chrono::high_resolution_clock::now()) _T0;
};

void
StopWatch::start()
{
    _T0 = std::chrono::high_resolution_clock::now();
}

double
StopWatch::now() const
{
    using namespace std::chrono;
    auto ns = duration_cast<nanoseconds>(high_resolution_clock::now() - _T0);
    return double(ns.count()) * 1e-9;
}

inline void
sleep(double secs)
{
    if (secs <= 0)
        return;
    using namespace std::chrono;
    std::this_thread::sleep_for(duration<double>(secs));
}
