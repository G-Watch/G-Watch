#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"


using gw_image_sign_t = uint64_t;


/*!
 *  \brief  image to for storing profile data (base class)
 */
class GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     */
    GWProfileImage();


    /*!
     *  \brief  deconstructor
     */
    ~GWProfileImage();


    /*!
     *  \brief  sign a image type based on list of metric names and chip name
     *  \param  metric_names    list of metric names
     *  \param  chip_name       chip name   
     *  \return sign value
     */
    static gw_image_sign_t sign(std::vector<std::string> metric_names, std::string chip_name);


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    virtual gw_retval_t fill(){ return GW_FAILED_NOT_IMPLEMENTAED; }


    /*!
     *  \brief  obtain the raw pointer to this image
     */
    inline uint8_t* data() const { return (uint8_t*)(this->_image.data()); }


    /*!
     *  \brief  obtain the size of this image
     */
    inline uint64_t size() const { return this->_image.size(); }

    
    /*!
     *  \brief  load this image from binary file
     *  \param  path    path to the binary file
     *  \return GW_SUCCESS for successfully load 
     */
    gw_retval_t load_from_file(std::string file_path);


    /*!
     *  \brief  store this image to binary file
     *  \param  path        path to the binary file
     *  \param  overwrite   whether to do overwrite
     *  \return GW_SUCCESS for successfully store 
     */
    gw_retval_t store_to_file(std::string file_path, bool overwrite=false) const;


 protected:
    // underlying area to store the binary image
    std::vector<uint8_t> _image;
};


/*!
 *  \brief  collection of profile images
 *  \note   TODO(zhuobin): we need to change this library design, 
 *          not all images should be stored in the library, e.g.,
 *          those not-recycle image such as counter data
 */
class GWProfileImageLibrary {
 public:
    /*!
     *  \brief  constructor
     */
    GWProfileImageLibrary();


    /*!
     *  \brief  deconstructor
     */
    ~GWProfileImageLibrary();
 

    /*!
     *  \brief  try obtain image by given signature
     *  \param  sign    signature of the image
     *  \param  image   the obtained image (if success)
     *  \return GW_SUCCESS for successfully obtained
     *          GW_FAILED_NOT_EXIST for no corresponding image stored
     */
    gw_retval_t get_image_by_sign(gw_image_sign_t sign, GWProfileImage** image);


    /*!
     *  \brief  store image
     *  \param  sign    signature of the image
     *  \param  image   the stored image
     *  \return GW_SUCCESS for successfully stored
     */
    gw_retval_t store_image(gw_image_sign_t sign, GWProfileImage* image);


 private:
    std::map<gw_image_sign_t, GWProfileImage*> _library;
};
