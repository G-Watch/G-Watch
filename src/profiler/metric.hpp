#pragma once

#include <iostream>
#include <vector>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"


/*!
 *  \brief  A specific metric, either collected from HW or calculated by SW afterward.
 *          A metric could be identified by its name and chip name.
 */
class GWProfileMetric {
 public:
    /*!
     *  \brief  constructor
     *  \param  metric_name name of the metric
     *  \param  chip_name   name of the chip
     */
    GWProfileMetric(std::string metric_name, std::string chip_name);


    /*!
     *  \brief  deconstructor 
     */
    ~GWProfileMetric();

    
    /*!
     *  \brief  get the name of the current metric
     *  \return name of the current metric
     */
    inline std::string get_metric_name() const { return this->_metric_name; }


    /*!
     *  \brief  get the name of the chip
     *  \return name of the chip
     */
    inline std::string get_chip_name() const { return this->_chip_name; }


 protected:
    // name of the current metric
    std::string _metric_name;

    // name of the chip
    std::string _chip_name;
};
