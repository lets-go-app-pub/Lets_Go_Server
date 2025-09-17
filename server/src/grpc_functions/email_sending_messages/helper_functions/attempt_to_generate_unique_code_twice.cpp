//
// Created by jeremiah on 8/16/21.
//

#include <optional>

#include <mongocxx/result/update.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility_general_functions.h>
#include <store_mongoDB_error_and_exception.h>

#include "helper_functions.h"


#include "database_names.h"
#include "collection_names.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

std::string generateRandomCode() {
    return gen_random_alpha_numeric_string(
            general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH
    );
}

bool attempt_to_generate_unique_code_twice(
        std::string& randomly_generated_code,
        mongocxx::collection& collection_to_insert_to,
        const bsoncxx::document::view& find_document,
        const std::function<bsoncxx::document::value(const std::string& code)>& generate_update_document
) {

    randomly_generated_code = generateRandomCode();

    bool inserted_successfully = false;

    bsoncxx::stdx::optional<mongocxx::result::update> updateDocumentResults;
    for (size_t i = 0; i < 2 && !inserted_successfully; i++) {

        bsoncxx::document::value update_document = generate_update_document(randomly_generated_code);

        std::optional<std::string> updateAccountExceptionString;
        try {

            mongocxx::options::update update_options;
            update_options.upsert(true);

            //upsert the pending account
            updateDocumentResults = collection_to_insert_to.update_one(
                    find_document,
                    update_document.view(),
                    update_options
            );

        } catch (const mongocxx::logic_error& e) {
            updateAccountExceptionString = e.what();
        } catch (const mongocxx::operation_exception& e) {
            if (e.code().value() == 11000) { //duplicate key error

                //If Verification Code already exists, this is possible, this should VERY rarely if ever happen. It
                // can happen because Verification Code AND another unique index exist inside the account and verification
                // code is not checked for.

                //generate code and retry
                randomly_generated_code = generateRandomCode();
                continue;
            } else { //NOT duplicate key error
                if (e.raw_server_error()) { //raw_server_error exists
                    throw mongocxx::operation_exception(
                            e.code(),
                            bsoncxx::document::value(e.raw_server_error().value()),
                            e.what());
                } else { //raw_server_error does not exist
                    throw mongocxx::operation_exception(
                            e.code(),
                            document{} << finalize,
                            e.what());
                }
            }
        }

        if (
            updateDocumentResults
            && (updateDocumentResults->result().upserted_count() > 0
                || updateDocumentResults->result().modified_count() > 0)
                ) { //document was upserted(inserted) or modified(updated)
            inserted_successfully = true;
        } else {
            std::string errorString = "Verification document could not be successfully inserted.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, updateAccountExceptionString, errorString,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_to_insert_to.name().to_string(),
                    "find_document", find_document,
                    "update_document", update_document.view()
            );
            return false;
        }
    }

    if (!inserted_successfully) {
        std::string errorString = "Verification document was not successfully inserted after multiple attempts.\n";

        std::optional<std::string> dummy_string;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, dummy_string, errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_to_insert_to.name().to_string(),
                "find_document", find_document,
                "final_code", randomly_generated_code
        );
        return false;
    }

    return true;

}