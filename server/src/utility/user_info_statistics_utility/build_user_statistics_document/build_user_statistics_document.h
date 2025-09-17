//
// Created by jeremiah on 11/8/21.
//

#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>

bsoncxx::document::value buildUserStatisticsDocument(
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::array::view& phone_number_array = bsoncxx::builder::basic::array{}
        );