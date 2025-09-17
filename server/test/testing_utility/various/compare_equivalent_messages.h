//
// Created by jeremiah on 10/5/22.
//

#pragma once

#include <iostream>
#include <google/protobuf/util/message_differencer.h>
#include "gtest/gtest.h"

template <typename T>
void compareEquivalentMessages(const T& first, const T& second) {
    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            first,
            second
    );

    if(!equivalent) {
        std::cout << "first\n" << first.DebugString() << '\n';
        std::cout << "second\n" << second.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}