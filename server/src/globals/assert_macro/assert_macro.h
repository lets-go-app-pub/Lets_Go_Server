//
// Created by jeremiah on 10/26/21.
//

#pragma once

#include <iostream>

#define assert_msg(condition, message)\
(!(condition)) ?\
(std::cerr << "Assertion failed: (" << #condition << "), "\
<< "\nFunction: " << __FUNCTION__\
<< "\nFile: " << __FILE__\
<< "\nLine: " << __LINE__ \
<< '\n' << (message) << '\n', abort(), 0) : 1
