#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <format>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <typeinfo>

#include <string.h>
#include <cxxabi.h>

#include "common/utils/exception.hpp"

/*!
 *  \brief  utilities to process string
 */
class GWUtilString {
 public:
    /*!
     *  \brief  merge a string list
     *  \param  vec         list of string to be merged
     *  \param  delimiter   delimiter
     *  \return merge result
     */
    static std::string merge(const std::vector<std::string>& vec, char delimiter='\n'){
        uint64_t i;
        std::string retval("");

        for(i=0; i<vec.size(); i++){
            retval += vec[i];
            if(i != vec.size() - 1){
                retval += std::to_string(delimiter);
            }
        }

        return retval;
    }


    static uint16_t string_to_uint16(const std::string& str){
        unsigned long result = std::stoul(str);
        if (result > UINT16_MAX) {
            throw std::out_of_range("value out of uint16_t range");
        }
        return static_cast<uint16_t>(result);
    }


    static std::string ptr_to_hex_string(const void* p) {
        std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
        std::ostringstream oss;
        oss << "0x"
            << std::hex << std::setw(sizeof(std::uintptr_t) * 2) << std::setfill('0')
            << v;
        return oss.str();
    }


    static std::string normalize_trim(std::string s){
        const char* ws = " \t\n\r\f\v";
        auto l = s.find_first_not_of(ws);
        if (l == std::string::npos)
            return std::string{};
        auto r = s.find_last_not_of(ws);
        s = s.substr(l, r - l + 1);

        return s;
    }

    static std::string to_lower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }


    static std::string demangle_name(const char* name) {
        int status = -1;
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name, NULL, NULL, &status),
            std::free
        };
        return (status == 0) ? res.get() : name;
    }


    static std::vector<std::string> split_by_delimiter(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }


    /*!
     *  \brief  convert istream to string
     *  \param  input   input stream
     *  \return string
     */
    static std::string istream_to_string(std::istream& input) {
        std::ostringstream ss;
        ss << input.rdbuf();
        return ss.str();
    }


    template<uint64_t length>
    static std::string hash(const std::string& input) {
        if (length == 0) return "";
        const std::string charset = "abcdefghijklmnopqrstuvwxyz"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789";
        uint64_t hash = 1469598103934665603ULL; // FNV64 offset basis
        const uint64_t prime = 1099511628211ULL; // FNV64 prime
        std::string result = "";

        // FNV-1a
        for (unsigned char c : input) {
            hash ^= c;
            hash *= prime;
        }

        result.reserve(length);
        for (size_t i = 0; i < length; i++) {
            result.push_back(charset[hash % charset.size()]);
            hash = (hash >> 1) ^ (hash * prime);
        }

        return result;
    }


    template<typename T>
    static std::string vector_to_string(const std::vector<T>& data, bool cast_hex=true, std::string spliter=",") {
        std::ostringstream oss;

        if(cast_hex){
            oss << std::hex;
        }

        for (size_t i = 0; i < data.size(); ++i) {
            if constexpr (!std::is_same_v<T, std::string>) {
                oss << std::to_string(data[i]);
            } else {
                oss << data[i];
            }
            if (i != data.size() - 1) {
                oss << std::format("{} ", spliter);
            }
        }
        return oss.str();
    }


    static std::string demangle(std::string mangled_name) {
        int status = -1;

        char* demangled_buffer = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
        if (status == 0) {
            std::string result(demangled_buffer);
            std::free(demangled_buffer);
            return result;
        } else {
            return mangled_name;
        }
    }
};
