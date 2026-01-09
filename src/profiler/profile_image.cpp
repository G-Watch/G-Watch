#include <iostream>
#include <filesystem>
#include <fstream>

#include "common/common.hpp"
#include "profiler/profiler.hpp"
#include "profiler/profile_image.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/hash.hpp"
#include "common/utils/string.hpp"


GWProfileImage::GWProfileImage(){}
GWProfileImage::~GWProfileImage(){}


gw_image_sign_t GWProfileImage::sign(std::vector<std::string> metric_names, std::string chip_name){
    gw_hash_u64_t hash;

    GW_ASSERT(metric_names.size() > 0);

    metric_names.push_back(chip_name);
    hash = GWUtilHash::cal(metric_names);

    return (gw_image_sign_t)(hash);
}


gw_retval_t GWProfileImage::load_from_file(std::string file_path){
    gw_retval_t retval = GW_SUCCESS;
    std::ifstream file;
    std::streamsize size;

    GW_ASSERT(file_path.size() > 0);
    if(unlikely(!std::filesystem::exists(file_path))){
        GW_WARN_C(
            "failed to load image from binary file, file not exists: file_path(%s)",
            file_path.c_str()
        );
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    file.open(file_path, std::ios::binary);
    if(!file){
        GW_WARN_C(
            "failed to load image from binary file, failed to open file: file_path(%s)",
            file_path.c_str()
        );
        retval = GW_FAILED;
        goto exit;
    }

    file.seekg(0, std::ios::end);
    size = file.tellg();
    file.seekg(0, std::ios::beg);

    this->_image.resize(size);
    if (!file.read(reinterpret_cast<char*>(this->_image.data()), size)) {
        GW_WARN_C(
            "failed to load image from binary file, failed to read file: file_path(%s)",
            file_path.c_str()
        );
        retval = GW_FAILED;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWProfileImage::store_to_file(std::string file_path, bool overwrite) const {
    gw_retval_t retval = GW_SUCCESS;
    std::ofstream file;

    GW_ASSERT(file_path.size() > 0);
    if(unlikely(std::filesystem::exists(file_path))){
        if(overwrite){
            GW_WARN_C("overwrite existed image binary file: file_path(%s)", file_path.c_str());
        } else {
            GW_WARN_C("omit store to binary image file, file already exist: file_path(%s)", file_path.c_str());
            retval = GW_FAILED_ALREADY_EXIST;
            goto exit;
        }
    }

    file.open(file_path, std::ios::binary);
    if(!file){
        GW_WARN_C(
            "failed to store image to binary file, failed to open file: file_path(%s)",
            file_path.c_str()
        );
        retval = GW_FAILED;
        goto exit;
    }

    if (!file.write(reinterpret_cast<const char*>(this->_image.data()), this->_image.size())) {
        GW_WARN_C(
            "failed to store image to binary file, failed to write file: file_path(%s)",
            file_path.c_str()
        );
        retval = GW_FAILED;
        goto exit;
    }

exit:
    return retval;
}


GWProfileImageLibrary::GWProfileImageLibrary(){}
GWProfileImageLibrary::~GWProfileImageLibrary(){}


gw_retval_t GWProfileImageLibrary::get_image_by_sign(gw_image_sign_t sign, GWProfileImage** image){
    gw_retval_t retval = GW_SUCCESS;
    GW_CHECK_POINTER(image);

    if(this->_library.count(sign) == 0){
        retval = GW_FAILED_NOT_EXIST;
    } else {
        *image = this->_library[sign];
    }

    return retval;
}


gw_retval_t GWProfileImageLibrary::store_image(gw_image_sign_t sign, GWProfileImage* image){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(image);

    if(this->_library.count(sign) == 0){
        this->_library[sign] = image;
    } else {
        GW_WARN_C("omit image store, already exist: sign(%lu)", sign);
        retval = GW_FAILED_ALREADY_EXIST;
    }

    return retval;
}
