//
// Created by jeremiah on 9/14/22.
//

#include <optional>
#include <mongocxx/exception/logic_error.hpp>

#include "mongocxx/collection.hpp"

#include "set_fields_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "store_mongoDB_error_and_exception.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//Expected to be run inside a try catch block for ErrorExtractingFromBsoncxx.
void addToMatchingElements(
        const bsoncxx::document::view& user_account_view,
        const std::string& array_key,
        bsoncxx::builder::basic::array& other_users_accounts
        ) {

    const bsoncxx::array::view other_users_matched_accounts_list = extractFromBsoncxx_k_array(
            user_account_view,
            array_key
    );

    for(const auto& ele : other_users_matched_accounts_list) {
        const bsoncxx::document::view matched_element_doc = extractFromBsoncxxArrayElement_k_document(
                ele
        );

        const bsoncxx::oid user_oid = extractFromBsoncxx_k_oid(
                matched_element_doc,
                user_account_keys::accounts_list::OID
        );

        other_users_accounts.append(user_oid);
    }
}

bool removeInvalidElementsForUpdatedMatchingParameters(
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& user_account_view,
        mongocxx::collection& user_accounts_collection,
        mongocxx::stdx::optional<bsoncxx::document::value>& final_user_account_doc,
        const bsoncxx::document::view& match_parameters_to_query,
        const bsoncxx::document::view& fields_to_update_document,
        const bsoncxx::document::view& fields_to_project
        ) {

    bsoncxx::builder::basic::array extracted_user_oids;

    try {

        addToMatchingElements(
                user_account_view,
                user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
                extracted_user_oids
        );

        addToMatchingElements(
                user_account_view,
                user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
                extracted_user_oids
        );

        const bsoncxx::array::view other_users_accounts_view = extracted_user_oids.view();

        //If no accounts are inside the matched lists, skip the database query.
        if (!other_users_accounts_view.empty()) {

            std::optional<std::string> exception_string;
            mongocxx::stdx::optional<mongocxx::cursor> find_matching_accounts;
            try {

                mongocxx::options::find opts;

                opts.projection(
                    document{}
                        << "_id" << 1
                    << finalize
                );

                bsoncxx::builder::stream::document query_doc;

                query_doc
                    << "_id" << open_document
                        << "$in" << other_users_accounts_view
                    << close_document
                    << bsoncxx::builder::concatenate(match_parameters_to_query);

                //find user account document
                find_matching_accounts = user_accounts_collection.find(
                        query_doc.view(),
                        opts
                );

            } catch (const mongocxx::logic_error& e) {
                exception_string = e.what();
            }

            if (!find_matching_accounts) {

                const std::string error_string = "Finding user accounts encountered an error";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        exception_string, error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "ObjectID_used", user_account_oid,
                        "match_parameters_to_query", match_parameters_to_query,
                        "fields_to_update_document", fields_to_update_document
                );

                return false;
            }

            extracted_user_oids.clear();

            for (const auto& match_account_doc: *find_matching_accounts) {
                bsoncxx::oid matching_account_oid = extractFromBsoncxx_k_oid(
                        match_account_doc,
                        "_id"
                );
                extracted_user_oids.append(matching_account_oid);
            }
        }

        std::optional<std::string> exception_string;
        try {
            mongocxx::options::find_one_and_update opts;

            bsoncxx::builder::stream::document projection_doc;

            projection_doc
                << "_id" << 1
                << bsoncxx::builder::concatenate(fields_to_project);

            opts.projection(projection_doc.view());

            opts.return_document(mongocxx::options::return_document::k_after);

            bsoncxx::builder::stream::document add_fields_document;

            appendDocumentToClearMatchingInfoAggregation(
                    add_fields_document,
                    extracted_user_oids
            );

            add_fields_document
                    << bsoncxx::builder::concatenate(fields_to_update_document);

            mongocxx::pipeline pipe;

            pipe.add_fields(add_fields_document.view());

            //find user account document
            final_user_account_doc = user_accounts_collection.find_one_and_update(
                    document{}
                            << "_id" << user_account_oid
                            << finalize,
                    pipe,
                    opts
            );

        } catch (const mongocxx::logic_error& e) {
            exception_string = e.what();
        }

        if(!final_user_account_doc) {
            const std::string error_string = "Finding or updating single user account encountered an error after "
                                             "account was already found once.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", user_account_oid,
                    "match_parameters_to_query", match_parameters_to_query,
                    "fields_to_update_document", fields_to_update_document
            );

            return false;
        }

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Error is already stored
        return false;
    }

    return true;

}