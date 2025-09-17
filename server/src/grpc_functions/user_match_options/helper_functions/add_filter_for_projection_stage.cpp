//
// Created by jeremiah on 3/19/21.
//

#include "user_match_options_helper_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void addFilterForProjectionStage(
        document& project_reduced_arrays_doc_builder,
        const bsoncxx::oid& match_oid,
        const std::string& array_name,
        const std::string& array_oid_element_name
) {

    project_reduced_arrays_doc_builder
            << array_name << open_document
                << "$filter" << open_document
                    << "input" << "$" + array_name
                    << "cond" << open_document
                        << "$eq" << open_array
                          << "$$this." + array_oid_element_name << match_oid
                        << close_array
                    << close_document
                << close_document
            << close_document;

}