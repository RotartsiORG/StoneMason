//
// Created by grant on 8/14/20.
//

#pragma once

#ifndef __STONEMASON_EXCEPT_HPP
#define __STONEMASON_EXCEPT_HPP

#define __STMS_DEFINE_EXCEPTION(x) class x : public std::runtime_error { \
public:                                                                  \
    explicit x(const std::string &str) : std::runtime_error(str) {}      \
}

#include <stdexcept>

namespace stms {
    class InternalException : public std::exception {};
}

#endif //__STONEMASON_EXCEPT_HPP
