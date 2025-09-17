//
// Created by jeremiah on 9/9/22.
//

#pragma once

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//Returns the number of elements in the bsoncxx::array::view inside matching_account_view.
//If store_error_when_elements_found is set to true every time an element is found an error will be stored.
//array_name is the field name of the array to be iterated through.
template <bool store_error_when_elements_found>
int checkForNoElementsInArray(
        bsoncxx::document::view* matching_account_view,
        const std::string& array_name
) {

    int num_elements = 0;

    try {
        bsoncxx::array::view other_user_match_array = extractFromBsoncxx_k_array(
                *matching_account_view,
                array_name
        );

        for (const auto& ele : other_user_match_array) {
            if (store_error_when_elements_found) {
                bsoncxx::document::view document = extractFromBsoncxxArrayElement_k_document(ele);

                const std::string error_string = "Too many elements in array '" + array_name + "'.";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "error_document", document
                );
            }
            num_elements++;
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Any errors have already been stored
        return -1;
    }

    return num_elements;
}