#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"


/*!
 *  \brief  initialize binary utilities
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_binary_utilities_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize cubin utilities
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_cubin_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize kernel def sass interface
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_kernel_def_sass_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize fatbin utilities
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_fatbin_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize ptx utilities
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_ptx_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize profile context interface
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_profile_context_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize profile device interface
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_profile_device_interface(pybind11::module_ &m);


/*!
 *  \brief  initialize profile profiler interface
 *  \param  m   python module
 */
void __gw_pybind_cuda_init_profile_profiler_interface(pybind11::module_ &m);
