#include <iostream>
#include <vector>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/metric.hpp"


GWProfileMetric::GWProfileMetric(std::string metric_name, std::string chip_name)
    : _metric_name(metric_name), _chip_name(chip_name)
{
    GW_ASSERT(this->_metric_name.length() > 0);
    GW_ASSERT(this->_chip_name.length() > 0);
}


GWProfileMetric::~GWProfileMetric(){}
