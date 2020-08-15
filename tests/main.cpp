//
// Created by grant on 1/2/20.
//

#include "gtest/gtest.h"
#include "stms/log_test.cpp"
#include "stms/async_test.cpp"
#include "stms/general.cpp"

#if STMS_SSL_TESTS_ENABLED  // Toggle SSL tests, disable for travis
#   include "stms/ssl_test.cpp"
#endif
