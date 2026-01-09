#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <cassert>

#include "common/common.hpp"
#include "common/log.hpp"


/*!
 *  \brief  utilities to handle numeric
 */
class GWUtilNumeric {
 public:
    /*!
     *  \brief  align up
     *  \param  x       input
     *  \param  align   align
     *  \return aligned value
     */
    template<typename T>
    static T align_up(T x, T align) {
        assert(align != 0);
        if (x > UINT64_MAX - (align - 1)) {
            return UINT64_MAX;
        }
        return ((x + align - 1) / align) * align;
    }


    template<typename T_signed, typename T_unsigned>
    static T_signed sign_extend(T_unsigned x, uint64_t bits) {
        assert(bits >= 1 && bits < sizeof(T_signed)*8);
        GW_STATIC_ASSERT(sizeof(T_signed) == sizeof(T_unsigned));
        x = (x << (sizeof(T_signed)*8 - bits));
        return std::bit_cast<T_signed>(x) >> (sizeof(T_signed)*8 - bits);
    }


    template<typename T>
    static T stoul(const std::string &s, int base = 0) {
        try {
            // base=0 -> auto-detect 0x/0 prefix
            unsigned long val = std::stoul(s, nullptr, base);
            if (val > std::numeric_limits<T>::max()){
                GW_ERROR_DETAIL(
                    "overflow error: given(%lu), max(%lu)",
                    val, std::numeric_limits<T>::max()
                );
            }
            return static_cast<T>(val);
        } catch (const std::invalid_argument&) {
            GW_ERROR_DETAIL("invalid argument: %s", s.c_str());
        } catch (const std::out_of_range&) {
            GW_ERROR_DETAIL("out of range: %s", s.c_str());
        }
    }


    template<typename T>
    static int compare_numeric_str(const std::string &a, const std::string &b, int base = 0){
        T va = GWUtilNumeric::stoul<T>(a, base);
        T vb = GWUtilNumeric::stoul<T>(b, base);
        if (va > vb) return 1;
        if (va < vb) return -1;
        return 0;
    }


    static std::vector<size_t> get_list_aligned_offsets(const std::vector<size_t>& member_sizes, size_t max_alignment, size_t bias = 0) {
        std::vector<size_t> offsets;
        size_t alignment;
        size_t offset = bias;
        size_t struct_alignment = 0;

        for (size_t size : member_sizes) {
            size_t alignment = std::min(size, max_alignment);
            struct_alignment = std::max(struct_alignment, alignment);

            if (offset % alignment != 0) {
                offset += alignment - (offset % alignment);
            }

            offsets.push_back(offset);
            offset += size;
        }

        if (offset % struct_alignment != 0) {
            offset += struct_alignment - (offset % struct_alignment);
        }

        return offsets;
    }
};
