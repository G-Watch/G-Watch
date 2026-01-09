#pragma once

#include <iostream>
#include <string>

#include <Python.h>


class GWUtilPython {
 public:
    GWUtilPython();
    ~GWUtilPython();

    /*!
     *  \brief  import a module from a string
     *  \param  code the code of the module
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t import_module_from_string(std::string& code);


 private:
    static gw_retval_t __init_python_env();
    static gw_retval_t __deinit_python_env();
};
