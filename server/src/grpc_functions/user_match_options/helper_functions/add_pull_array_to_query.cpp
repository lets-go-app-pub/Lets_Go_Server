//
// Created by jeremiah on 3/19/21.
//

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void addPullArrayToQuery(
        const bsoncxx::oid& match_oid,
        const bsoncxx::document::view& document_view,
        bsoncxx::builder::stream::document& project_reduced_arrays_doc_builder,
        const std::string& array_list_key,
        const std::string& array_list_oid_key,
        int number_elements
) {

    if (number_elements != 0) { //if return val is non-zero (-1 means error, but was already stored)

        if (number_elements > 1) { //if more than 1 element exists in this array

            const std::string error_string = "Too many elements in array " + array_list_key + "'.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "matching_document", document_view
            );
        }

        //add query to clear list
        project_reduced_arrays_doc_builder
                << "$pull" << open_document
                    << array_list_key << open_document
                        << array_list_oid_key << match_oid
                    << close_document
                << close_document;
    }
}