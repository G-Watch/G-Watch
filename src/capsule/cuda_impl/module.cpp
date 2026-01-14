#include <iostream>
#include <vector>
#include <mutex>
#include <filesystem>

#include <cuda.h>
// #include <gelf.h>
#include <string.h>
#include <libwebsockets.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/assemble/kernel_def.hpp"
#include "common/utils/string.hpp"
#include "common/utils/cuda.hpp"
#include "common/cuda_impl/binary/cubin.hpp"
#include "common/cuda_impl/binary/fatbin.hpp"
#include "common/cuda_impl/binary/ptx.hpp"
#include "common/cuda_impl/binary/utils.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"
#include "capsule/capsule.hpp"
#include "scheduler/serve/capsule_message.hpp"


gw_retval_t GWCapsule::CUDA_cache_culibrary(CUlibrary library, std::string file_path_str){
    gw_retval_t retval = GW_SUCCESS;
    std::filesystem::path file_path;
    std::vector<uint8_t> buffer;
    std::ifstream in;

    file_path = file_path_str;
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        GW_WARN_C("failed to cache CUlibrary due to no file exist: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    buffer.reserve(static_cast<uint64_t>(std::filesystem::file_size(file_path)));

    in.open(file_path, std::ios::binary);
    if (!in.is_open()) {
        GW_WARN_C("failed to cache CUlibrary due to failed to open file: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    buffer.resize(static_cast<size_t>(std::filesystem::file_size(file_path)));
    in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (!in) {
        GW_WARN_C("failed to cache CUlibrary due to failed to read file: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_SDK;
        goto exit;
    }

    retval = this->CUDA_cache_culibrary(library, buffer.data());

exit:
    if(in.is_open()){
        in.close();
    }
    return retval;
}


gw_retval_t GWCapsule::CUDA_cache_culibrary(CUlibrary library, const void *data) {
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard lock_guard(this->_mutex_module_management);

    GW_ASSERT(library != (CUlibrary)0);
    this->_map_culibrary_data.insert({ library, {} });

    GW_IF_FAILED(
        GWBinaryImageExt_CUDAFatbin::extract_from_byte_sequence(data, this->_map_culibrary_data[library]),
        retval,
        {
            GW_WARN_C(
                "failed to cache CUDA library, failed to extract fatbinary from raw byte sequence: error(%s)",
                gw_retval_str(retval)
            );
            goto exit;
        }
    );
    GW_DEBUG_C("cached CUDA library: CUlibrary(%p), size(%lu)", library, this->_map_culibrary_data[library].size());

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_cache_cumodule(CUmodule module, std::string file_path_str){
    gw_retval_t retval = GW_SUCCESS;
    std::filesystem::path file_path;
    std::vector<uint8_t> buffer;
    std::ifstream in;

    file_path = file_path_str;
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        GW_WARN_C("failed to cache CUmodule due to no file exist: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    buffer.reserve(static_cast<uint64_t>(std::filesystem::file_size(file_path)));

    in.open(file_path, std::ios::binary);
    if (!in.is_open()) {
        GW_WARN_C("failed to cache CUmodule due to failed to open file: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    buffer.resize(static_cast<size_t>(std::filesystem::file_size(file_path)));
    in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (!in) {
        GW_WARN_C("failed to cache CUmodule due to failed to read file: file_path(%s)", file_path_str.c_str());
        retval = GW_FAILED_SDK;
        goto exit;
    }

    retval = this->CUDA_cache_cumodule(module, buffer.data());

exit:
    if(in.is_open()){
        in.close();
    }
    return retval;
}


gw_retval_t GWCapsule::CUDA_cache_cumodule(CUmodule module, const void *data){
    gw_retval_t retval = GW_SUCCESS;
    CUcontext cu_context = (CUcontext)0;
    std::lock_guard lock_guard(this->_mutex_module_management);

    GW_IF_FAILED(
        GWUtilCUDA::get_current_cucontext(cu_context),
        retval,
        goto exit;
    );
    GW_ASSERT(cu_context != (CUcontext)0);
    GW_ASSERT(module != (CUmodule)0);

    // we need to avoid the CUmodule be duplicated record due to a call to cuLibraryGetModule
    if(this->_map_cumodule_culibrary[cu_context].count(module) > 0){
        GW_WARN_C(
            "CUmodule has already been recorded during cuLibraryGetModule, skip recording it again: "
            "CUcontext(%p), CUmodule(%p)",
            cu_context, module
        );
        goto exit; 
    }
    this->_map_cumodule_data[cu_context].insert({ module, {} });

    GW_IF_FAILED(
        GWBinaryImageExt_CUDAFatbin::extract_from_byte_sequence(
            data,
            this->_map_cumodule_data[cu_context][module]
        ),
        retval,
        {
            GW_WARN_C(
                "failed to cache CUDA module, failed to extract fatbinary from raw byte sequence: error(%s)",
                gw_retval_str(retval)
            );
            goto exit;
        }
    );
    GW_DEBUG_C(
        "cached CUDA module: CUcontext(%p), CUmodule(%p), size(%lu)",
        cu_context, module, this->_map_cumodule_data[cu_context][module].size()
    );

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_record_mapping_cumodule_culibrary(CUmodule module, CUlibrary library){
    gw_retval_t retval = GW_SUCCESS;
    CUcontext cu_context = (CUcontext)0;
    std::lock_guard lock_guard(this->_mutex_module_management);
    
    GW_IF_FAILED(
        GWUtilCUDA::get_current_cucontext(cu_context),
        retval,
        goto exit;
    );
    GW_ASSERT(cu_context != (CUcontext)0);
    GW_ASSERT(module!= (CUmodule)0);
    GW_ASSERT(library!= (CUlibrary)0);

    // we need to avoid the CUmodule be duplicated record due to a call to cuModuleLoadData, etc.
    if(this->_map_cumodule_data[cu_context].count(module) > 0){
        GW_WARN_C(
            "CUmodule has already been recorded during cuModuleLoadData etc., skip recording it again: "
            "CUcontext(%p), CUmodule(%p), CUlibrary(%p)",
            cu_context, module, library
        );
        goto exit; 
    }

    this->_map_cumodule_culibrary[cu_context].insert({ module, library });
    GW_DEBUG_C(
        "recorded mapping between CUmodule and CUlibrary: "
        "CUcontext(%p), CUmodule(%p), CUlibrary(%p)",
        cu_context, module, library
    );

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_record_mapping_cufunction_cumodule(CUfunction function, CUmodule module){
    gw_retval_t retval = GW_SUCCESS;
    CUcontext cu_context = (CUcontext)0;

    GW_IF_FAILED(
        GWUtilCUDA::get_current_cucontext(cu_context),
        retval,
        goto exit;
    );
    GW_ASSERT(cu_context != (CUcontext)0);
    GW_ASSERT(function != (CUfunction)0);
    GW_ASSERT(module != (CUmodule)0);

    {
        std::lock_guard lock_guard(this->_mutex_module_management);
        if(this->_map_cufunction_cumodule[cu_context].count(function) > 0){
            // shouldn't happend
            GW_WARN_C(
                "CUfunction-CUmodule mapping has already been recorded: "
                "CUcontext(%p), CUfunction(%p), CUmodule(%p)",
                cu_context, function, module
            );
            goto exit; 
        }
        this->_map_cufunction_cumodule[cu_context].insert({ function, module });
    }

    GW_DEBUG_C(
        "recorded mapping between CUfunction and CUmodule: "
        "CUcontext(%p), CUfunction(%p), CUmodule(%p)",
        cu_context, function, module
    );

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_parse_cufunction(
    CUfunction function, CUmodule module, bool do_parse_entire_binary
){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    uint64_t i = 0, nb_ptx = 0, nb_cubin = 0;
    CUresult cudv_retval = CUDA_SUCCESS;
    CUlibrary culibrary = (CUlibrary)0;
    CUcontext cu_context = (CUcontext)0;
    GWKernelDef *kerneldef = nullptr;
    GWKernelDefExt_CUDA_SASS *kerneldef_ext_sass = nullptr;
    GWInternalMessage_Capsule *message_write_sql = nullptr;
    GWInternalMessagePayload_Common_DB_SQL_Write *sql_write_payload = nullptr;
    GWBinaryUtility_CUDA::gw_cuda_binary_t binary_type = GWBinaryUtility_CUDA::GW_CUDA_BINARY_UNKNOWN;
    char string_buffer[2048] = { 0 };
    std::string current_arch_version = "", cubin_arch_version = "", kernel_arch_version = "";
    std::string mangled_name = "";
    std::vector<uint8_t> *binary_data = nullptr;
    const char *func_name = nullptr;
    GWBinaryImage *binary_fatbin = nullptr;
    GWBinaryImage *binary_cubin = nullptr;
    GWBinaryImage *binary_ptx = nullptr;
    GWBinaryImageExt_CUDAFatbin *binary_ext_fatbin = nullptr;
    GWBinaryImageExt_CUDACubin *binary_ext_cubin = nullptr;
    GWBinaryImageExt_CUDAPTX *binary_ext_ptx = nullptr;
    typename std::multimap<CUmodule, GWBinaryImage*>::iterator map_iter;
    bool found_kerneldef = false, is_fatbin_first_seen = false;
    std::lock_guard lock_guard(this->_mutex_module_management);

    // obtain current context
    GW_IF_FAILED(
        GWUtilCUDA::get_current_cucontext(cu_context),
        retval,
        goto exit;
    );
    GW_ASSERT(cu_context != (CUcontext)0);

    // obtain the arch version of current device
    GW_IF_FAILED(
        GWUtilCUDA::get_arch_of_current_context(current_arch_version),
        retval,
        {
            GW_WARN_C(
                "failed to parse CUfunction due to failed to get arch version of current device: "
                "CUfunction(%p), CUmodule(%p), function(%s)",
                function, module, mangled_name.c_str()
            );
            goto exit;
        }
    )

    // obtain the function name
    mangled_name.reserve(8192);
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetName(&func_name, function),
        cudv_retval,
        {
            GW_WARN_C(
                "failed to parse CUfunction due to failed to get mangled name: "
                "CUfunction(%p), CUmodule(%p)",
                function, module
            );
            goto exit;
        }
    );
    mangled_name = func_name;

    // check whether the function has already been parsed
    // i.e. call cuModuleGetFunction on the same function before
    if(this->_map_cufunction_kerneldef[cu_context].count(function) > 0){
        goto exit;
    }

    // check whether the module has been recorded
    if(unlikely(
            this->_map_cumodule_culibrary[cu_context].count(module) == 0
        and this->_map_cumodule_data[cu_context].count(module) == 0)
    ){
        GW_WARN_C(
            "failed to parse CUfunction due to missing CUmodule record, "
            "failed to found the record of neither its parent CUlibrary nor CUmodule itself "
            "CUcontext(%p), CUfunction(%p), CUmodule(%p), function(%s)",
            cu_context, function, module, mangled_name.c_str()
        );
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    if(unlikely(
            this->_map_cumodule_culibrary[cu_context].count(module) > 0
        and this->_map_cumodule_data[cu_context].count(module) > 0
    )){
        GW_ERROR_C("duplicated record of CUmodule, this is a bug: CUmodule(%p)", module);
    }

    if(this->_map_cumodule_culibrary[cu_context].count(module) > 0){
        // obtain origin library
        culibrary = this->_map_cumodule_culibrary[cu_context][module];

        // get the raw data byte sequence
        if(unlikely(this->_map_culibrary_data.count(culibrary) == 0)){
            GW_WARN_C(
                "failed to parse CUfunction due to missing CUlibrary record, "
                "failed to found the record of parent CUlibrary of the CUmodule"
                "CUfunction(%p), CUmodule(%p), CUlibrary(%p), function(%s)",
                function, module, culibrary, mangled_name.c_str()
            );
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }
        binary_data = &(this->_map_culibrary_data[culibrary]);
    } else if (this->_map_cumodule_data[cu_context].count(module) > 0){
        // get the raw data byte sequence
        binary_data = &(this->_map_cumodule_data[cu_context][module]);
    }
    GW_CHECK_POINTER(binary_data);

    // obtain the type of binary
    GW_IF_FAILED(
        GWBinaryUtility_CUDA::get_binary_type(binary_data->data(), binary_data->size(), binary_type),
        retval,
        {
            GW_WARN_C(
                "failed to parse CUfunction due to failed to obtain binary type: "
                "CUfunction(%p), CUmodule(%p), function(%s)", 
                function, module, mangled_name.c_str()
            );
            goto exit;
        }
    )
    GW_ASSERT(binary_type != GWBinaryUtility_CUDA::GW_CUDA_BINARY_UNKNOWN);

    // parse the binary
    if(binary_type == GWBinaryUtility_CUDA::GW_CUDA_BINARY_FATBIN){
        if(this->_map_cumodule_fatbin[cu_context].count(module) > 0){
            GW_CHECK_POINTER(binary_fatbin = this->_map_cumodule_fatbin[cu_context][module]);
            GW_CHECK_POINTER(binary_ext_fatbin = GWBinaryImageExt_CUDAFatbin::get_ext_ptr(binary_fatbin));
        } else {
            GW_CHECK_POINTER(binary_ext_fatbin = GWBinaryImageExt_CUDAFatbin::create());
            GW_CHECK_POINTER(binary_fatbin = binary_ext_fatbin->get_base_ptr());
            GW_IF_FAILED(
                binary_fatbin->fill(binary_data->data(), binary_data->size()),
                retval,
                {
                    GW_WARN_C(
                        "failed to parse CUfunction due to failed to fill fatbin: "
                        "error(%s), "
                        "CUcontext(%p), CUfunction(%p), CUmodule(%p), function(%s)",
                        gw_retval_str(retval),
                        cu_context, function, module, mangled_name.c_str()
                    )
                    goto exit;
                }
            );

            // build the map: <module, fatbin>
            this->_map_cumodule_fatbin[cu_context].insert({ module, binary_fatbin });
            GW_DEBUG_C(
                "recorded fatbin from CUmodule: CUcontext(%p), CUmodule(%p), Fatbin(%p)",
                cu_context, module, binary_fatbin
            );

            is_fatbin_first_seen = true;
        }

        // parse the fatbin (this would trigger JIT compile all PTXs)
        GW_IF_FAILED(
            binary_fatbin->parse(),
            retval,
            {
                GW_WARN_C(
                    "failed to parse CUfunction due to failed to parse fatbin: "
                    "error(%s), "
                    "CUcontext(%p), CUfunction(%p), CUmodule(%p), function(%s)",
                    gw_retval_str(retval),
                    cu_context, function, module, mangled_name.c_str()
                )
                goto exit;
            }
        );

        // record the ptx inside the fatbin
        for(i=0; i<binary_ext_fatbin->params().list_ptx.size(); i++){
            GW_CHECK_POINTER(binary_ptx = binary_ext_fatbin->params().list_ptx[i]);

            if(is_fatbin_first_seen == true){
                // build the map: <module, ptx>
                this->_map_cumodule_ptx[cu_context].insert({ module, binary_ptx });
                GW_DEBUG(
                    "recorded PTX from CUmodule: CUcontext(%p), CUmodule(%p), PTX(%p)",
                    cu_context, module, binary_ptx
                );
            }

            // parse the ptx
            if(do_parse_entire_binary == true){
                GW_IF_FAILED(
                    binary_ptx->parse(),
                    retval,
                    {
                        GW_WARN_C(
                            "failed to parse CUfunction due to failed to parse ptx: "
                            "CUcontext(%p), CUfunction(%p), CUmodule(%p), function(%s)",
                            cu_context, function, module, mangled_name.c_str()
                        )
                        goto exit;
                    }
                );
            }
        }

        // record the cubin inside the fatbin
        for(i=0; i<binary_ext_fatbin->params().list_cubin.size(); i++){
            GW_CHECK_POINTER(binary_cubin = binary_ext_fatbin->params().list_cubin[i]);
            binary_ext_cubin = GWBinaryImageExt_CUDACubin::get_ext_ptr(binary_cubin);
            GW_CHECK_POINTER(binary_ext_cubin);

            // obtain the arch version of the cubin
            GW_IF_FAILED(
                binary_ext_cubin->get_arch_version_from_byte_sequence(cubin_arch_version),
                retval,
                {
                    GW_WARN("failed to extract cubin arch version: error(%s)", gw_retval_str(tmp_retval));
                    goto exit;
                }
            );

            // build the map: <module, cubin>
            if(is_fatbin_first_seen == true){
                this->_map_cumodule_cubin[cu_context].insert({ module, binary_cubin });
                GW_DEBUG(
                    "recorded CUBIN from CUmodule: CUcontext(%p), CUmodule(%p), CUBIN(%p), arch_version(%s)",
                    cu_context, module, binary_cubin, cubin_arch_version.c_str()
                );
            }

            if(do_parse_entire_binary == true){
                // parse the cubin
                GW_IF_FAILED(
                    binary_cubin->parse(),
                    retval,
                    {
                        GW_WARN_C(
                            "failed to parse CUfunction due to failed to parse cubin: "
                            "CUcontext(%p), CUfunction(%p), CUmodule(%p), function(%s)",
                            cu_context, function, module, mangled_name.c_str()
                        )
                        goto exit;
                    }
                );

                // build the map: <function, kerneldef>
                if(GWBinaryUtility_CUDA::is_arch_equal(
                    /* arch_version_1 */ cubin_arch_version,
                    /* arch_version_2 */ current_arch_version,
                    /* ignore_variant_suffix */ true
                )){
                    tmp_retval = binary_ext_cubin->get_kerneldef_by_name(mangled_name, kerneldef);
                    if(tmp_retval == GW_SUCCESS){
                        if(found_kerneldef == true){
                            // NOTE(zhuobin): shouldn't happen
                            GW_WARN_C(
                                "duplicated CUfunction from CUmodule, is this normal?: "
                                "CUcontext(%p), CUmodule(%p), CUfunction(%p), function(%s)",
                                cu_context, module, function, mangled_name.c_str()
                            );
                        }

                        GW_CHECK_POINTER(kerneldef);

                        // record to map
                        this->_map_cufunction_kerneldef[cu_context].insert({ function, kerneldef });
                        found_kerneldef = true;
                        GW_DEBUG(
                            "recorded CUfunction from CUmodule: CUcontext(%p), CUmodule(%p), CUfunction(%p), arch_version(%s)",
                            cu_context, module, function, cubin_arch_version.c_str()
                        );

                        // cast to extension interface
                        kerneldef_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kerneldef);
                        GW_CHECK_POINTER(kerneldef_ext_sass);
                        kerneldef_ext_sass->params().cu_function = function;

                        // check kerneldef architecture version
                        kernel_arch_version = kerneldef_ext_sass->params().arch_version;
                        GW_ASSERT(
                            GWBinaryUtility_CUDA::is_arch_equal(
                                /* arch_version_1 */ current_arch_version,
                                /* arch_version_2 */ kernel_arch_version,
                                /* ignore_variant_suffix */ false
                            )
                        );
                    }
                }
            } else { // do_parse_entire_binary == false
                if(found_kerneldef == false){
                    if(GWBinaryUtility_CUDA::is_arch_equal(
                        /* arch_version_1 */ cubin_arch_version,
                        /* arch_version_2 */ current_arch_version,
                        /* ignore_variant_suffix */ true
                    )){
                        tmp_retval = binary_ext_cubin->extract_kerneldef_from_byte_sequence(mangled_name, kerneldef);
                        if(tmp_retval == GW_SUCCESS){
                            GW_CHECK_POINTER(kerneldef);

                            // record to map
                            this->_map_cufunction_kerneldef[cu_context].insert({ function, kerneldef });
                            found_kerneldef = true;
                            GW_DEBUG(
                                "recorded CUfunction from CUmodule: CUcontext(%p), CUmodule(%p), CUfunction(%p), arch_version(%s)",
                                cu_context, module, function, cubin_arch_version.c_str()
                            );

                            // cast to extension interface
                            kerneldef_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kerneldef);
                            GW_CHECK_POINTER(kerneldef_ext_sass);
                            
                            // check kerneldef architecture version
                            kernel_arch_version = kerneldef_ext_sass->params().arch_version;
                            GW_ASSERT(
                                GWBinaryUtility_CUDA::is_arch_equal(
                                    /* arch_version_1 */ current_arch_version,
                                    /* arch_version_2 */ kernel_arch_version,
                                    /* ignore_variant_suffix */ false
                                )
                            );
                        }
                    } // if is_arch_equal
                } // if found_kerneldef == false
            } // if do_parse_entire_binary == false
        } // forall CUBINs
    } else if (binary_type == GWBinaryUtility_CUDA::GW_CUDA_BINARY_CUBIN){
        if(this->_map_cumodule_cubin[cu_context].count(module) > 0){
            GW_ASSERT(this->_map_cumodule_cubin[cu_context].count(module) == 1);
            map_iter = this->_map_cumodule_cubin[cu_context].find(module);
            GW_CHECK_POINTER(binary_cubin = map_iter->second);
            GW_CHECK_POINTER(binary_ext_cubin = GWBinaryImageExt_CUDACubin::get_ext_ptr(binary_cubin))
            GW_IF_FAILED(
                binary_ext_cubin->get_arch_version_from_byte_sequence(cubin_arch_version),
                retval,
                {
                    GW_WARN("failed to extract cubin arch version: error(%s)", gw_retval_str(tmp_retval));
                    goto exit;
                }
            );
        } else {
            GW_CHECK_POINTER(binary_ext_cubin = GWBinaryImageExt_CUDACubin::create());
            GW_CHECK_POINTER(binary_cubin = binary_ext_cubin->get_base_ptr());

            GW_IF_FAILED(
                binary_cubin->fill(
                    /* bytes */ binary_data->data(),
                    /* byte_size */ binary_data->size()
                ),
                retval,
                {
                    GW_WARN_C(
                        "failed to fill cubin: "
                        "error(%s), "
                        "CUcontext(%p), CUmodule(%p)",
                        gw_retval_str(retval), cu_context, module
                    );
                    goto exit;
                }
            );
            this->_map_cumodule_cubin[cu_context].insert({ module, binary_cubin });

            // obtain cubin version
            GW_IF_FAILED(
                binary_ext_cubin->get_arch_version_from_byte_sequence(cubin_arch_version),
                retval,
                {
                    GW_WARN("failed to extract cubin arch version: error(%s)", gw_retval_str(tmp_retval));
                    goto exit;
                }
            );
            GW_DEBUG(
                "recorded CUBIN from CUmodule: CUcontext(%p), CUmodule(%p), CUBIN(%p), arch_version(%s)",
                cu_context, module, binary_cubin, cubin_arch_version.c_str()
            );
        }

        if(do_parse_entire_binary == true){
            GW_IF_FAILED(
                binary_cubin->parse(),
                retval,
                {
                    GW_WARN_C(
                        "failed to parse CUfunction due to failed to parse cubin: "
                        "CUfunction(%p), CUmodule(%p)",
                        function, module
                    )
                    goto exit;
                }
            );

            if(GWBinaryUtility_CUDA::is_arch_equal(
                /* arch_version_1 */ cubin_arch_version,
                /* arch_version_2 */ current_arch_version,
                /* ignore_variant_suffix */ true
            )){
                tmp_retval = binary_ext_cubin->get_kerneldef_by_name(mangled_name, kerneldef);
                if(tmp_retval == GW_SUCCESS){
                    // record to map
                    this->_map_cufunction_kerneldef[cu_context].insert({ function, kerneldef });
                    found_kerneldef = true;
                    GW_DEBUG(
                        "recorded CUfunction from CUmodule: CUcontext(%p), CUmodule(%p), CUfunction(%p), arch_version(%s)",
                        cu_context, module, function, binary_ext_cubin->params().arch_version.c_str()
                    );

                    // cast to extension interface
                    kerneldef_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kerneldef);
                    GW_CHECK_POINTER(kerneldef_ext_sass);
                    kerneldef_ext_sass->params().cu_function = function;

                    // check kerneldef architecture version
                    kernel_arch_version = kerneldef_ext_sass->params().arch_version;
                    GW_ASSERT(
                        GWBinaryUtility_CUDA::is_arch_equal(
                            /* arch_version_1 */ current_arch_version,
                            /* arch_version_2 */ kernel_arch_version,
                            /* ignore_variant_suffix */ false
                        )
                    );
                }
            } // is_arch_equal
        } else { // do_parse_entire_binary == false
            if(GWBinaryUtility_CUDA::is_arch_equal(
                /* arch_version_1 */ cubin_arch_version,
                /* arch_version_2 */ current_arch_version,
                /* ignore_variant_suffix */ true
            )){
                tmp_retval = binary_ext_cubin->get_kerneldef_by_name(mangled_name, kerneldef);
                if(tmp_retval == GW_SUCCESS){
                    // record to map
                    this->_map_cufunction_kerneldef[cu_context].insert({ function, kerneldef });
                    found_kerneldef = true;
                    GW_DEBUG(
                        "recorded CUfunction from CUmodule: CUcontext(%p), CUmodule(%p), CUfunction(%p), arch_version(%s)",
                        cu_context, module, function, binary_ext_cubin->params().arch_version.c_str()
                    );

                    // cast to extension interface
                    kerneldef_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kerneldef);
                    GW_CHECK_POINTER(kerneldef_ext_sass);
                    kerneldef_ext_sass->params().cu_function = function;

                    // check kerneldef architecture version
                    kernel_arch_version = kerneldef_ext_sass->params().arch_version;
                    GW_ASSERT(
                        GWBinaryUtility_CUDA::is_arch_equal(
                            /* arch_version_1 */ current_arch_version,
                            /* arch_version_2 */ kernel_arch_version,
                            /* ignore_variant_suffix */ false
                        )
                    );
                } // gw_cubin_get_kerneldef_by_name
            } // is_arch_equal
        } // do_parse_entire_binary
    } else {
        GW_ERROR_C_DETAIL("unknown binary type, this is a bug: binary type(%u)", binary_type);
    }

    // return error if kerneldef isn't found
    if(unlikely(found_kerneldef == false)){
        GW_WARN_C(
            "failed to parse CUfunction due to failed to find the function in the binary: "
            "CUfunction(%p), CUmodule(%p), binary_type(%s), function(%s), cubin_arch_version(%s), current_arch_version(%s)",
            function,
            module,
            [](GWBinaryUtility_CUDA::gw_cuda_binary_t type){
                switch(type){
                    case GWBinaryUtility_CUDA::GW_CUDA_BINARY_FATBIN:
                        return "FATBIN";
                    case GWBinaryUtility_CUDA::GW_CUDA_BINARY_CUBIN:
                        return "CUBIN";
                    case GWBinaryUtility_CUDA::GW_CUDA_BINARY_PTX:
                        return "PTX";
                    default:
                        return "UNKNOWN";
                }
            }(binary_type),
            mangled_name.c_str(),
            cubin_arch_version.c_str(),
            current_arch_version.c_str()
        );
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    } else {
        GW_CHECK_POINTER(kerneldef);
        GW_ASSERT(kerneldef->is_instructions_parsed());
        GW_ASSERT(kerneldef->is_cfg_parsed());
    }

    // get arch_version and mangled_name of kerneldef_sass
    kerneldef_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kerneldef);
    GW_CHECK_POINTER(kerneldef_ext_sass);
    kernel_arch_version = kerneldef_ext_sass->params().arch_version;

    // report to scheduler
    // record trace id to SQL database
    GW_CHECK_POINTER(message_write_sql = new GWInternalMessage_Capsule());
    sql_write_payload = message_write_sql->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Write>(
        GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB
    );
    GW_CHECK_POINTER(sql_write_payload);
    message_write_sql->type_id = GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB;
    sql_write_payload->table_name = "mgnt_cuda_kernel_overview";
    sql_write_payload->insert_data = {
        { "mangled_name", mangled_name },
        { "cu_function", GWUtilString::ptr_to_hex_string((const void*)(function)) },
        { "arch", kernel_arch_version }
    };
    tmp_retval = this->send_to_scheduler(message_write_sql);
    if(tmp_retval == GW_SUCCESS){
        GW_DEBUG("sent CUDA kernel (SQL) to scheduler: function(%p)", function);
    }

 exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_report_function(CUfunction function){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;
    GWInternalMessage_Capsule capsule_message;
    GWInternalMessagePayload_Common_DB_KV_Write *payload;

    payload = capsule_message.get_payload_ptr<GWInternalMessagePayload_Common_DB_KV_Write>(GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    gw_cuda_function_attribute_t *trace_function = nullptr;
    const char* mangled_name = nullptr;
    std::string demangled_name;
    int ptx_version = 0;
    int sass_version = 0;
    int max_thread_per_block = 0;
    int static_smem_size = 0;
    int max_dynamic_smem_size = 0;
    int const_mem_size = 0;
    int local_mem_size = 0;
    int num_reg = 0;

    std::lock_guard<std::mutex>(this->_mutex_module_management);

    if(unlikely(this->_map_trace_function.count(function) > 0)){
        GW_CHECK_POINTER(trace_function = this->_map_trace_function[function]);
        GW_WARN_C(
            "recording an already-recorded function, old function would be overwritten: "
            "cufunction(%p), function_name(%s)",
            function, trace_function->demangled_name.c_str()
        );
        this->_map_trace_function.erase(function);
        delete trace_function;
    }

    // obtain function attributes
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&ptx_version, CU_FUNC_ATTRIBUTE_PTX_VERSION, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&sass_version, CU_FUNC_ATTRIBUTE_BINARY_VERSION, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&max_thread_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&static_smem_size, CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&max_dynamic_smem_size, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&const_mem_size, CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&local_mem_size, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetAttribute(&num_reg, CU_FUNC_ATTRIBUTE_NUM_REGS, function),
        cudv_retval, goto exit;
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuFuncGetName(&mangled_name, function),
        cudv_retval, goto exit;
    );
    demangled_name = GWUtilString::demangle_name(mangled_name);

    GW_CHECK_POINTER(
        trace_function = new gw_cuda_function_attribute_t(
            function,
            demangled_name,
            ptx_version,
            sass_version,
            max_thread_per_block,
            static_smem_size,
            max_dynamic_smem_size,
            const_mem_size,
            local_mem_size,
            num_reg
        )
    );
    this->_map_trace_function[function] = trace_function;

    capsule_message.type_id = GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB;
    payload->uri = "/capsule/" + this->global_id + "/kernels/" + mangled_name;
    payload->write_payload = gw_cuda_function_attribute_t(
        function,
        demangled_name,
        ptx_version,
        sass_version,
        max_thread_per_block,
        static_smem_size,
        max_dynamic_smem_size,
        const_mem_size,
        local_mem_size,
        num_reg
    );
    tmp_retval = this->send_to_scheduler(&capsule_message);
    if(tmp_retval == GW_SUCCESS){
        GW_DEBUG("sent function report to scheduler: function(%p)", function);
    }

    GW_DEBUG(
        "recorded function: function(%p), demangled_name(%s), sass_version(%d), ptx_version(%d), max_thread_per_block(%d), static_smem_size(%d), max_dynamic_smem_size(%d), const_mem_size(%d), local_mem_size(%d), num_reg(%d)",
        function, demangled_name.c_str(),
        sass_version, ptx_version,
        max_thread_per_block,
        static_smem_size,
        max_dynamic_smem_size,
        const_mem_size,
        local_mem_size,
        num_reg
    );

exit:
    return retval;
}
