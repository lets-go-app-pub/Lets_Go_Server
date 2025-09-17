//
// Created by jeremiah on 6/4/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <activity_suggestion_feedback_keys.h>

#include "feedback_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void ActivitySuggestionFeedbackDoc::generateRandomValues(bool random_marked_by_spam) {
    current_object_oid = bsoncxx::oid{};
    account_oid = bsoncxx::oid{};
    activity_name = gen_random_alpha_numeric_string(rand() % 50 + 10);
    message = gen_random_alpha_numeric_string(rand() % 150 + 20);
    timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}};

    if(random_marked_by_spam && rand() % 2) {
        marked_as_spam = std::make_unique<std::string>(bsoncxx::oid{}.to_string());
    } else {
        marked_as_spam = nullptr;
    }
};

//converts this ActivitySuggestionFeedbackDoc object to a document and saves it to the passed builder
void ActivitySuggestionFeedbackDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << activity_suggestion_feedback_keys::ACCOUNT_OID << account_oid
            << activity_suggestion_feedback_keys::ACTIVITY_NAME << activity_name
            << activity_suggestion_feedback_keys::MESSAGE << message
            << activity_suggestion_feedback_keys::TIMESTAMP_STORED << timestamp_stored;

    if(marked_as_spam) {
        document_result
            << activity_suggestion_feedback_keys::MARKED_AS_SPAM << *marked_as_spam;
    }

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool ActivitySuggestionFeedbackDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];
    mongocxx::collection activity_suggestion_feedback_collection = feedback_db[collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            activity_suggestion_feedback_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = activity_suggestion_feedback_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ActivitySuggestionFeedbackDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitySuggestionFeedbackDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}


bool ActivitySuggestionFeedbackDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool ActivitySuggestionFeedbackDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                << "_id" << find_oid
            << finalize
    );
}

bool ActivitySuggestionFeedbackDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];
    mongocxx::collection activity_suggestion_feedback_collection = feedback_db[collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = activity_suggestion_feedback_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ActivitySuggestionFeedbackDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitySuggestionFeedbackDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool ActivitySuggestionFeedbackDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function ActivitySuggestionFeedbackDoc::saveInfoToDocument\n";
        return false;
    }
}

bool
ActivitySuggestionFeedbackDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        account_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                activity_suggestion_feedback_keys::ACCOUNT_OID
        );

        activity_name = extractFromBsoncxx_k_utf8(
                user_account_document,
                activity_suggestion_feedback_keys::ACTIVITY_NAME
        );

        message = extractFromBsoncxx_k_utf8(
                user_account_document,
                activity_suggestion_feedback_keys::MESSAGE
        );

        timestamp_stored = extractFromBsoncxx_k_date(
                user_account_document,
                activity_suggestion_feedback_keys::TIMESTAMP_STORED
        );

        auto element = user_account_document[activity_suggestion_feedback_keys::MARKED_AS_SPAM];
        if (element && element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
            marked_as_spam = std::make_unique<std::string>(element.get_string().value.to_string());
        } else if(!element) {
            marked_as_spam = nullptr;
        } else { //if element does not exist or is not type utf8

            logElementError(__LINE__, __FILE__, element,
                            user_account_document, bsoncxx::type::k_utf8, activity_suggestion_feedback_keys::MARKED_AS_SPAM,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            throw ErrorExtractingFromBsoncxx("Error requesting user info value " + activity_suggestion_feedback_keys::MARKED_AS_SPAM + ".");
        }

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitySuggestionFeedbackDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ActivitySuggestionFeedbackDoc::operator==(const ActivitySuggestionFeedbackDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            account_oid.to_string(),
            other.account_oid.to_string(),
            "ACCOUNT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            activity_name,
            other.activity_name,
            "ACTIVITY_NAME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            message,
            other.message,
            "MESSAGE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            timestamp_stored,
            other.timestamp_stored,
            "TIMESTAMP_STORED",
            OBJECT_CLASS_NAME,
            return_value
    );

    if(marked_as_spam && other.marked_as_spam) {
        checkForEquality(
                *marked_as_spam,
                *other.marked_as_spam,
                "MARKED_AS_SPAM",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                marked_as_spam,
                other.marked_as_spam,
                "MARKED_AS_SPAM existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ActivitySuggestionFeedbackDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
