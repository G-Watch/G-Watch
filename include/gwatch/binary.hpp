#pragma once

#include <iostream>
#include <vector>

#include "gwatch/common.hpp"


namespace gwatch {


/*!
 *  \brief  binary image
 */
class BinaryImage {
 public:
    BinaryImage();
    ~BinaryImage();


    /*!
     *  \brief  fill in the binary data
     *  \param  data        pointer to the binary data
     *  \param  size        size of the binary data, if the size if 0,
     *                      then the binary data will be parsed based
     *                      on the given binary data
     *  \return gwSuccess for successfully fill
     */
    virtual gwError fill(const void *data, uint64_t size){ return gwFailed; }


     /*!
      *  \brief  parse the binary image
      *  \note   this function should be called after calling fill
      *  \return gwSuccess for successfully parse
      */
    virtual gwError parse(){ return gwFailed; }
};


} // namespace gwatch
