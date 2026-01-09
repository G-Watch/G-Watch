#pragma once

#include <iostream>
#include <exception>
#include <string>
#include <sstream>
#include <cstdarg>


class GWException : public std::exception {
 public:
    /*!
     *  \brief (de)construtor
     */
    GWException(const std::string& message) : _msg(message) {}

    GWException(const char* format, ...) {
        va_list args;
        char buffer[8192] = {0};

        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        this->_msg = buffer;
    }

    ~GWException() = default;

    virtual const char* what() const noexcept override {
        return this->_msg.c_str();
    }

 private:
    // exception message
    std::string _msg;
};
