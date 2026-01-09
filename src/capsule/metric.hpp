#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"


class GWAppMetricTrace {
 public:
    GWAppMetricTrace(std::string name, uint64_t begin_hash, std::string line_position="")
        :   _name(name),
            _begin_hash(begin_hash), _end_hash(0),
            _s_tick(0), _e_tick(0),
            _line_position(line_position)
    {}
    ~GWAppMetricTrace() = default;

    inline void mark_begin_hash(uint64_t hash){ this->_begin_hash = hash; }
    inline void mark_end_hash(uint64_t hash){ this->_end_hash = hash; }
    inline uint64_t get_begin_hash() const { return this->_begin_hash; }
    inline uint64_t get_end_hash() const { return this->_end_hash; }

    inline void start_capture(){ this->_s_tick = GWUtilTscTimer::get_tsc(); }
    inline void stop_capture(){ this->_e_tick = GWUtilTscTimer::get_tsc(); }
    inline uint64_t get_s_tick() const { return this->_s_tick; }
    inline uint64_t get_e_tick() const { return this->_e_tick; }

 private:
    std::string _name;
    uint64_t _begin_hash, _end_hash;
    uint64_t _s_tick, _e_tick;
    std::string _line_position;
};
