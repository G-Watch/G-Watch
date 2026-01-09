#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <cassert>

#include <string.h>

#include "common/common.hpp"
#include "common/log.hpp"


/*!
 *  \brief  utilities to calculate hash value
 */
class GWUtilBytes {
 public:
    /*!
     *  \brief  merge a list of bytes into a single value
     *  \tparam T           type of the merged value
     *  \tparam big_edian   whether to use big endian
     *  \param  bytes       list of bytes to be merged
     *  \return merged value
     */
    template<typename T, bool big_edian>
    static T merge_bytes(const std::vector<uint8_t>& bytes) {
        T result = 0;
        uint64_t i;
        // GW_DEBUG("bytes.size(): %lu", bytes.size());
        // for(i = 0; i < bytes.size(); ++i) {
        //     GW_DEBUG("bytes[%lu]: %u", i, bytes[i]);
        // }
        // GW_DEBUG("sizeof(T): %lu", sizeof(T));
        assert(bytes.size() <= sizeof(T));
        for (i = 0; i < bytes.size(); ++i) {
            if constexpr (big_edian) {
                result <<= 8;
                result |= bytes[i];
            } else {
                result |= static_cast<T>(bytes[i]) << (8 * i);
            }
        }
        return result;
    }

    
    enum ByteEndian { LittleEndian = 0, BigEndian = 1 };


    /*!
     *  \brief  set a range of bits in a byte sequence
     *  \param  bytes       byte sequence to modify
     *  \param  lsb_id      least significant bit index
     *  \param  hsb_id      most significant bit index
     *  \param  bit_len     total bit length of the byte sequence
     *  \param  value       input byte sequence containing bits to set
     */
    template <ByteEndian Endian>
    static gw_retval_t set_bits(
        uint8_t* bytes,
        uint64_t lsb_id,
        uint64_t hsb_id,
        uint64_t bit_len,
        const std::vector<uint8_t>& value
    ){
        gw_retval_t retval = GW_SUCCESS;
        uint64_t bit_count, src_i, dst_i, byte_idx, reversed_dst_i;
        uint8_t  bit_offset, bit;
        
        GW_CHECK_POINTER(bytes);

        // validate lsb and hsb id
        if (lsb_id > hsb_id){
            GW_WARN("failed to set bits, lsb_id > hsb_id: lsb_id(%lu), hsb_id(%lu)", lsb_id, hsb_id);
            retval = GW_FAILED_INVALID_INPUT;
            return retval;
        }

        bit_count = hsb_id - lsb_id + 1;
        
        // Check if value has enough bits
        if (value.size() < (bit_count + 7) / 8) {
            GW_WARN("failed to set bits, input value too small for requested bit range");
            retval = GW_FAILED_INVALID_INPUT;
            return retval;
        }

        for (dst_i = lsb_id; dst_i <= hsb_id; dst_i++) {
            src_i = dst_i - lsb_id;
            
            // Get bit from source (value)
            bit = (value[src_i / 8] >> (src_i % 8)) & 1;
            
            // Set bit in destination (bytes)
            if constexpr (Endian == LittleEndian) {
                byte_idx = dst_i / 8;
                bit_offset = dst_i % 8;
            } else {
                reversed_dst_i = bit_len - dst_i - 1;
                byte_idx = reversed_dst_i / 8;
                bit_offset = reversed_dst_i % 8;
            }
            
            // Clear the target bit first
            bytes[byte_idx] &= ~(1 << bit_offset);
            
            // Set the bit with the new value
            bytes[byte_idx] |= (bit << bit_offset);
        }

    exit:
        return retval;
    }


    /*!
     *  \brief  set multiple ranges of bits in a byte sequence from a concatenated value
     *  \tparam Endian      byte endian for bit setting
     *  \tparam Ascending   if true, sort bit ranges by lsb ascending; if false, sort by lsb descending
     *  \param  bytes            byte sequence to modify
     *  \param  bit_ranges       list of bit ranges [any order endpoints]
     *  \param  bit_len          total bit length of the target byte sequence
     *  \param  value            input byte sequence containing concatenated bit values to set
     */
    template <ByteEndian Endian, bool Ascending = true>
    static gw_retval_t set_multi_ranges_bits(
        uint8_t* bytes,
        std::vector<std::pair<uint64_t, uint64_t>>& bit_ranges,  // list of [any order endpoints]
        uint64_t bit_len,
        const std::vector<uint8_t>& value
    ){
        gw_retval_t retval = GW_SUCCESS;

        GW_CHECK_POINTER(bytes);

        if (bit_ranges.empty()) {
            GW_WARN("failed to set bits, empty bit ranges list");
            return GW_FAILED_INVALID_INPUT;
        }

        // 1) 规范化区间为 (lsb, hsb)
        std::vector<std::pair<uint64_t, uint64_t>> normalized_ranges;
        normalized_ranges.reserve(bit_ranges.size());

        uint64_t total_bits = 0;
        for (const auto& range : bit_ranges) {
            uint64_t a = range.first;
            uint64_t b = range.second;
            uint64_t lsb_id = std::min(a, b);
            uint64_t hsb_id = std::max(a, b);

            if (hsb_id >= bit_len) {
                GW_WARN("failed to set bits, hsb(%lu) exceeds bit_len(%lu)", hsb_id, bit_len);
                return GW_FAILED_INVALID_INPUT;
            }

            normalized_ranges.emplace_back(lsb_id, hsb_id);
            total_bits += (hsb_id - lsb_id + 1);
        }

        // 2) 排序（按 lsb），顺序由 Ascending 控制
        std::sort(
            normalized_ranges.begin(),
            normalized_ranges.end(),
            [](const std::pair<uint64_t, uint64_t>& x, const std::pair<uint64_t, uint64_t>& y) {
                if constexpr (Ascending) {
                    if (x.first != y.first) return x.first < y.first;
                    return x.second < y.second;
                } else {
                    if (x.first != y.first) return x.first > y.first;
                    return x.second > y.second;
                }
            }
        );

        // 3) 检查 value 长度是否足够
        if (value.size() < (total_bits + 7) / 8) {
            GW_WARN("failed to set bits, input value too small for requested bit ranges");
            return GW_FAILED_INVALID_INPUT;
        }

        // 4) 按排序后的区间顺序，从 value 中按位切分并写入 bytes
        uint64_t value_bit_position = 0;
        for (const auto& range : normalized_ranges) {
            uint64_t lsb_id = range.first;
            uint64_t hsb_id = range.second;
            uint64_t range_bit_count = hsb_id - lsb_id + 1;

            // 从 value 中拷贝这段到临时缓冲（位从 LSB first packing）
            std::vector<uint8_t> temp_value((range_bit_count + 7) / 8, 0);
            for (uint64_t i = 0; i < range_bit_count; ++i) {
                uint64_t src_byte_idx = value_bit_position / 8;
                uint8_t src_bit_offset = static_cast<uint8_t>(value_bit_position % 8);

                if (src_byte_idx >= value.size()) {
                    GW_WARN("value too small while slicing bits");
                    return GW_FAILED_INVALID_INPUT;
                }

                uint8_t bit = static_cast<uint8_t>((value[src_byte_idx] >> src_bit_offset) & 1u);

                uint64_t dst_byte_idx = i / 8;
                uint8_t dst_bit_offset = static_cast<uint8_t>(i % 8);

                temp_value[dst_byte_idx] |= static_cast<uint8_t>(bit << dst_bit_offset);

                ++value_bit_position;
            }

            // 使用 set_bits（它期望 lsb <= hsb）
            retval = set_bits<Endian>(bytes, lsb_id, hsb_id, bit_len, temp_value);
            if (retval != GW_SUCCESS) {
                return retval;
            }
        }

        return retval;
    }


    template <ByteEndian Endian, bool Ascending = true>
    static gw_retval_t set_multi_ranges_bits(
        uint8_t* bytes,
        std::vector<std::pair<uint64_t, uint64_t>>&& bit_ranges,
        uint64_t bit_len,
        const std::vector<uint8_t>& value
    ){
        // 将右值移动到一个本地 lvalue 容器再调用（保持接口一致性）
        std::vector<std::pair<uint64_t, uint64_t>> local_ranges = std::move(bit_ranges);
        return set_multi_ranges_bits<Endian, Ascending>(bytes, local_ranges, bit_len, value);
    }


    /*!
     *  \brief  extract a range of bits from a byte sequence
     *  \param  bytes       byte sequence
     *  \param  lsb_id      least significant bit index
     *  \param  hsb_id      most significant bit index
     *  \param  result      output byte sequence
     */
    template <ByteEndian Endian>
    static gw_retval_t extract_bits(
        const uint8_t* bytes,
        uint64_t lsb_id,
        uint64_t hsb_id,
        uint64_t bit_len,
        std::vector<uint8_t>& result
    ){
        gw_retval_t retval = GW_SUCCESS;
        uint64_t bit_count, src_i, dst_i, byte_idx, reversed_src_i, byte_count;
        uint8_t  bit_offset, bit;
        
        GW_CHECK_POINTER(bytes);

        // validate lsb and hsb id
        if (lsb_id > hsb_id){
            GW_WARN("failed to extract bits, lsb_id > hsb_id: lsb_id(%lu), hsb_id(%lu)", lsb_id, hsb_id);
            retval = GW_FAILED_INVALID_INPUT;
            return retval;
        }


        bit_count = hsb_id - lsb_id + 1;
        byte_count = (bit_count + 7) / 8;

        result.clear();
        result.resize(byte_count);

        for (src_i = lsb_id; src_i <= hsb_id; src_i++) {
            dst_i = src_i - lsb_id;

            if constexpr (Endian == LittleEndian) {       
                byte_idx = src_i / 8;         
                bit_offset = src_i % 8;
            } else {
                reversed_src_i = bit_len - src_i;
                byte_idx = reversed_src_i / 8;         
                bit_offset = reversed_src_i % 8;
            }

            bit = (bytes[byte_idx] >> bit_offset) & 1;
            result[dst_i / 8] |= uint8_t(bit << (dst_i % 8));
        }

    exit:
        return retval;
    }
    

    template <ByteEndian Endian, bool Ascending = true>
    static gw_retval_t extract_multi_ranges_bits(
        const uint8_t* bytes,
        std::vector<std::pair<uint64_t, uint64_t>>& bit_ranges,
        uint64_t bit_len,
        std::vector<uint8_t>& result
    ){
        gw_retval_t retval = GW_SUCCESS;

        GW_CHECK_POINTER(bytes);

        if (bit_ranges.empty()) {
            GW_WARN("failed to extract bits, empty bit ranges list");
            return GW_FAILED_INVALID_INPUT;
        }

        // 1) 规范化区间为 (lsb, hsb)
        std::vector<std::pair<uint64_t, uint64_t>> normalized_ranges;
        normalized_ranges.reserve(bit_ranges.size());

        uint64_t total_bits = 0;
        for (const auto& range : bit_ranges) {
            uint64_t a = range.first;
            uint64_t b = range.second;
            uint64_t lsb_id = std::min(a, b);
            uint64_t hsb_id = std::max(a, b);

            if (hsb_id >= bit_len) {
                GW_WARN("failed to extract bits, hsb(%lu) exceeds bit_len(%lu)", hsb_id, bit_len);
                return GW_FAILED_INVALID_INPUT;
            }

            normalized_ranges.emplace_back(lsb_id, hsb_id);
            total_bits += (hsb_id - lsb_id + 1);
        }

        // 2) 按 lsb 排序，升序/降序由 Ascending 控制
        std::sort(
            normalized_ranges.begin(),
            normalized_ranges.end(),
            [](const std::pair<uint64_t, uint64_t>& x, const std::pair<uint64_t, uint64_t>& y) {
                if constexpr (Ascending) {
                    if (x.first != y.first) return x.first < y.first;
                    return x.second < y.second;
                } else {
                    if (x.first != y.first) return x.first > y.first;
                    return x.second > y.second;
                }
            }
        );

        // 3) 准备结果缓冲区
        uint64_t result_byte_count = (total_bits + 7) / 8;
        result.clear();
        result.resize(result_byte_count, 0);

        // 4) 按排序后的顺序提取并拼接
        uint64_t result_bit_position = 0;
        for (const auto& range : normalized_ranges) {
            uint64_t lsb_id = range.first;
            uint64_t hsb_id = range.second;
            uint64_t range_bit_count = hsb_id - lsb_id + 1;

            std::vector<uint8_t> temp_result;
            retval = extract_bits<Endian>(bytes, lsb_id, hsb_id, bit_len, temp_result);
            if (retval != GW_SUCCESS) {
                return retval;
            }

            for (uint64_t i = 0; i < range_bit_count; ++i) {
                uint64_t src_byte_idx = i / 8;
                uint8_t src_bit_offset = static_cast<uint8_t>(i % 8);
                uint8_t bit = static_cast<uint8_t>((temp_result[src_byte_idx] >> src_bit_offset) & 1u);

                uint64_t dst_byte_idx = result_bit_position / 8;
                uint8_t dst_bit_offset = static_cast<uint8_t>(result_bit_position % 8);

                result[dst_byte_idx] |= static_cast<uint8_t>(bit << dst_bit_offset);
                ++result_bit_position;
            }
        }

        return retval;
    }


    /*!
     *  \brief  extract multiple ranges of bits from a byte sequence and concatenate them
     *  \param  bytes            byte sequence
     *  \param  bit_ranges       list of bit ranges [hsb_id, lsb_id]
     *  \param  bit_len          total bit length of the input byte sequence
     *  \param  result           output byte sequence containing concatenated bit ranges
     */
    template <ByteEndian Endian, bool Ascending = true>
    static gw_retval_t extract_multi_ranges_bits(
        const uint8_t* bytes,
        std::vector<std::pair<uint64_t, uint64_t>>&& bit_ranges,
        uint64_t bit_len,
        std::vector<uint8_t>& result
    ){
        return extract_multi_ranges_bits<Endian, Ascending>(bytes, bit_ranges, bit_len, result);
    }


    template<typename T = uint64_t, ByteEndian Endian>
    static T byte_array_to_decimal(const std::vector<uint8_t>& bytes) {
        GW_STATIC_ASSERT(std::is_integral<T>::value);

        T result = 0;
        
        if constexpr (Endian == BigEndian) {
            for (size_t i = 0; i < bytes.size(); ++i) {
                result = (result << 8) | bytes[i];
            }
        } else {
            for (size_t i = 0; i < bytes.size(); ++i) {
                result |= static_cast<T>(bytes[i]) << (i * 8);
            }
        }

        return result;
    }

    template<typename T = uint64_t, ByteEndian Endian>
    static std::vector<uint8_t> decimal_to_byte_array(T value, std::size_t byte_count) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");

        std::vector<uint8_t> bytes(byte_count);

        if constexpr (Endian == BigEndian) {
            // 从最高字节到最低字节
            for (std::size_t i = 0; i < byte_count; ++i) {
                // byte_count - 1 - i：从最高位开始
                bytes[i] = static_cast<uint8_t>( (value >> ((byte_count - 1 - i) * 8)) & 0xFF );
            }
        } else {
            // 小端：从最低字节到最高字节
            for (std::size_t i = 0; i < byte_count; ++i) {
                bytes[i] = static_cast<uint8_t>( (value >> (i * 8)) & 0xFF );
            }
        }

        return bytes;
    }

    /*!
     *  \brief  print a byte sequence
     *  \param  p   byte sequence
     *  \param  n   size of byte sequence
     */
    static void print_bytes(const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; ++i)
            std::cout << "0x" << std::hex << int(p[i]) << " ";
        std::cout << std::dec << "\n";
    }

    static gw_retval_t dump_bytes_to_file(const std::vector<uint8_t>& data, const std::string& filename) {
        gw_retval_t retval = GW_SUCCESS;

        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            GW_WARN("failed to open file: %s", filename.c_str());
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }

        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!ofs) {
            GW_WARN("failed to write file: %s", filename.c_str());
            retval = GW_FAILED;
            goto exit;
        }
    
     exit:
        return retval;
    }


    static uint64_t read_uleb128(const uint8_t **data) {
        uint64_t result = 0;
        int shift = 0;
        uint8_t byte;
        
        do {
            byte = **data;
            (*data)++;
            result |= (uint64_t)(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        
        return result;
    }


    static int64_t read_sleb128(const uint8_t **data) {
        int64_t result = 0;
        int shift = 0;
        uint8_t byte;
        
        do {
            byte = **data;
            (*data)++;
            result |= (uint64_t)(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        
        if (shift < 64 && (byte & 0x40)) {
            result |= -(1LL << shift);
        }
        
        return result;
    }


    static const char* read_string(const uint8_t **data) {
        const char *str = (const char*)*data;
        *data += strlen(str) + 1;
        return str;
    }
};
