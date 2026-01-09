#pragma once
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "common/common.hpp"

/*!
 *  \brief  HPET-based timer
 *  \note   we provide HPET-based timer mainly for measuring the frequency of TSC
 *          more accurately, note that HPET is expensive to call
 */
class GWUtilHpetTimer {
 public:
    GWUtilHpetTimer(){}
    ~GWUtilHpetTimer() = default;

    /*!
     *  \brief  start timing
     */
    inline void start(){
        this->_start_time = std::chrono::high_resolution_clock::now();
    }

    /*!
     *  \brief  stop timing and obtain duration (ns)
     *  \return duration (ns)
     */
    inline double stop_get_ns() const {
        return static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - this->_start_time
            ).count()
        );
    }

    /*!
     *  \brief  stop timing and obtain duration (us)
     *  \return duration (us)
     */
    inline double stop_get_us() const {
        return stop_get_ns() / 1e3;
    }

    /*!
     *  \brief  stop timing and obtain duration (ms)
     *  \return duration (ms)
     */
    inline double stop_get_ms() const {
        return stop_get_ns() / 1e6;
    }

    /*!
     *  \brief  stop timing and obtain duration (s)
     *  \return duration (s)
     */
    inline double stop_get_s() const {
        return stop_get_ns() / 1e9;
    }

 private:
    // start time of the timing
    std::chrono::time_point<std::chrono::high_resolution_clock> _start_time;
};


/*!
 *  \brief  TSC-based timer
 */
class GWUtilTscTimer {
 public:
    GWUtilTscTimer(){ this->update_tsc_freq(); }


    ~GWUtilTscTimer() = default;


    /*!
     *  \brief  ontain TSC tick
     *  \return TSC tick
     */
    static inline uint64_t get_tsc(){
        uint64_t a, d;
        __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
        return (d << 32) | a;
    }


    /*!
     *  \brief  obtain time and TSC tick
     *  \return std::pair<std::string, uint64_t> (time, TSC tick)
     */
    static inline std::pair<std::string, uint64_t> get_time_and_tsc(){
        std::chrono::time_point<std::chrono::high_resolution_clock> hpet_now;
        uint64_t tsc_now;
        std::ostringstream oss;

        tsc_now = GWUtilTscTimer::get_tsc();
        hpet_now = std::chrono::system_clock::now();
        auto since_epoch = hpet_now.time_since_epoch();
        auto seconds_part = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
        auto micros_part  = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds_part);
        auto now_time_t = std::chrono::system_clock::to_time_t(hpet_now);
        std::tm local_tm = *std::localtime(&now_time_t);

        oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setw(6) << std::setfill('0') << micros_part.count();

        return std::pair<std::string, uint64_t>(oss.str(), tsc_now);
    }


    /*!
     *  \brief  update the TSC frequency
     */
    inline void update_tsc_freq(){
        GWUtilHpetTimer hpet;
        uint64_t sum = 5;

        hpet.start();

        // Do not change this loop! The hardcoded value below depends on this loop
        // and prevents it from being optimized out.
        const uint64_t rdtsc_start = this->get_tsc();
        for (uint64_t i = 0; i < 1000000; i++) {
            sum += i + (sum + i) * (i % sum);
        }
        GW_ASSERT(sum == 13580802877818827968ull);
        const uint64_t rdtsc_cycles = this->get_tsc() - rdtsc_start;

        this->_tsc_freq_g = rdtsc_cycles * 1.0 / hpet.stop_get_ns();
        this->_tsc_freq = this->_tsc_freq_g * 1000000000;
    }


    /*!
     *  \brief  calculate from tick range to duration (ms)
     *  \param  e_tick  end tick
     *  \param  s_tick  start tick
     *  \return duration (ms)
     */
    inline double tick_range_to_ms(uint64_t e_tick, uint64_t s_tick){
        return (double)(e_tick - s_tick) / (double) this->_tsc_freq * (double)1000.0f;
    }


    /*!
     *  \brief  calculate from tick range to duration (us)
     *  \param  e_tick  end tick
     *  \param  s_tick  start tick
     *  \return duration (us)
     */
    inline double tick_range_to_us(uint64_t e_tick, uint64_t s_tick){
        return (double)(e_tick - s_tick) / (double) this->_tsc_freq * (double)1000000.0f;
    }


    /*!
     *  \brief  calculate from duration (ms) to tick steps
     *  \param  duration  duration (ms)
     *  \return tick steps
     */
    inline double ms_to_tick(uint64_t duration){
        return (double)(duration) / (double)1000.0f * (double) this->_tsc_freq;
    }


    /*!
     *  \brief  calculate from duration (us) to tick steps
     *  \param  duration  duration (us)
     *  \return tick steps
     */
    inline double us_to_tick(uint64_t duration){
        return (double)(duration) / (double)1000000.0f * (double) this->_tsc_freq;
    }


    /*!
     *  \brief  calculate from tick steps to duration (ms)
     *  \param  tick steps 
     *  \return duration  duration (ms)
     */
    inline double tick_to_ms(uint64_t ticks){
        return (double)(ticks) * (double)1000.0f / (double) this->_tsc_freq;
    }


    /*!
     *  \brief  calculate from tick steps to duration (us)
     *  \param  tick steps 
     *  \return duration  duration (us)
     */
    inline double tick_to_us(uint64_t ticks){
        return (double)(ticks) * (double)1000000.0f / (double) this->_tsc_freq;
    }


    /*!
     *  \brief  obtain TSC frequency
     *  \return TSC frequency (GHz)
     */
    inline double get_tsc_freq(){
        return this->_tsc_freq;
    }

 private:
    // frequency of TSC register
    double _tsc_freq_g;
    double _tsc_freq;
};
