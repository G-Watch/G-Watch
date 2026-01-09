#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <cassert>
#include <fstream>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <sstream>

#include <limits.h>
#include <unistd.h>

#include "common/common.hpp"
#include "common/log.hpp"


/*!
 *  \brief  utilities to calculate hash value
 */
class GWUtilFileSystem {
 public:
    static gw_retval_t dump_bytes_to_file(const std::vector<uint8_t>& data, const std::string filename){
        gw_retval_t retval = GW_SUCCESS;

        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            GW_WARN("failed to open file to dump: path(%s)", filename.c_str());
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());

    exit:
        return retval;
    }

    static gw_retval_t read_all_from_fd(int fd, std::vector<uint8_t> &out, uint64_t total_size) {
        gw_retval_t retval = GW_SUCCESS;
        uint64_t pos = 0;
    
        out.clear();
        try {
            out.resize(total_size);
        } catch (std::bad_alloc &e) {
            GW_WARN("resize failed: %s", e.what()); 
            retval = GW_FAILED;
            goto exit;
        }
    
        while (pos < total_size) {
            // read in chunks not exceeding SSIZE_MAX to be safe
            size_t to_read = static_cast<size_t>(std::min<uint64_t>(
                total_size - pos,
                static_cast<uint64_t>(SSIZE_MAX)
            ));
            ssize_t r = read(fd, out.data() + pos, to_read);
            if (r < 0) {
                GW_WARN("read() failed at pos=%lu errno=%d (%s)", pos, errno, std::strerror(errno));
                retval = GW_FAILED;
                goto exit;
            }
            if (r == 0) {
                // EOF before we read expected bytes
                GW_WARN("unexpected EOF after %lu bytes (expected %lu)", pos, total_size);
                retval = GW_FAILED;
                goto exit;
            }
            pos += static_cast<uint64_t>(r);
        }

    exit:
        return retval;
    }
};
