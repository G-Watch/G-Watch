#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "common/utils/exception.hpp"


// forward declaration
class GWKernelDef;


/*!
 *  \brief  represent a GPU kernel instance
 */
class GWKernel {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWKernel(GWKernelDef* def);


    /*!
     *  \brief  destructor
     */
    virtual ~GWKernel();


    /*!
     *  \brief  assignment operator
     */
    GWKernel& operator=(const GWKernel& other) {
        if (this != &other) {
            this->assign(other);
        }
        return *this;
    }


    /*!
     *  \brief  obtain the definition of the current kernel instance
     *  \return definition of the current kernel instance
     */
    inline GWKernelDef* get_def(){ return this->_def; }

    
    /*!
     *  \brief  obtain number of thread launched by this kernel
     *  \return number of thread launched by this kernel
     */
    inline uint64_t get_nb_threads(){
        return this->grid_dim_x * this->grid_dim_y * this->grid_dim_z * this->block_dim_x * this->block_dim_y * this->block_dim_z;
    }


    // launch shape
    uint32_t grid_dim_x = 1;
    uint32_t grid_dim_y = 1;
    uint32_t grid_dim_z = 1;
    uint32_t block_dim_x = 1;
    uint32_t block_dim_y = 1;
    uint32_t block_dim_z = 1;

    // usage of shared memory
    uint32_t shared_mem_bytes = 0;

    // the stream to launch the kernel
    uint64_t stream = 0;

 protected:
    // the underlying kernel of this instance
    GWKernelDef* _def;


    /*!
     *  \brief  assignment implementation
     */
    virtual void assign(const GWKernel& other) {
        this->_def = other._def;
        this->grid_dim_x = other.grid_dim_x;
        this->grid_dim_y = other.grid_dim_y;
        this->grid_dim_z = other.grid_dim_z;
        this->block_dim_x = other.block_dim_x;
        this->block_dim_y = other.block_dim_y;
        this->block_dim_z = other.block_dim_z;
        this->shared_mem_bytes = other.shared_mem_bytes;
        this->stream = other.stream;
    }
    /* ==================== Common ==================== */
};
