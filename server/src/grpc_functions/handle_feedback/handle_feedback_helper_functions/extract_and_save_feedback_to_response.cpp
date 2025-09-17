//
// Created by jeremiah on 9/3/21.
//

#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <store_mongoDB_error_and_exception.h>

#include "handle_feedback_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"

//mongoDB
void addCapToResponse(
        handle_feedback::GetFeedbackResponse* response,
        const int number_to_request,
        const std::vector<handle_feedback::FeedbackElement*>& feedback_vector
);

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool extractAndSaveFeedbackToResponse(
        handle_feedback::GetFeedbackResponse* response,
        FeedbackType feedback_type,
        const NextPreviousFeedback feedback_next_previous,
        const int number_to_request,
        const std::chrono::milliseconds& last_time_feedback_extracted,
        const std::string& feedback_collection_name,
        const std::string& feedback_timestamp_field_name,
        const std::string& feedback_message_field_name,
        const std::string& feedback_sent_by_user_oid,
        const std::string& feedback_marked_as_spam_by_admin_name_field_name,
        mongocxx::collection& match_results_collection
) {

    mongocxx::stdx::optional<mongocxx::cursor> feedback_cursor;
    try {

        mongocxx::options::find opts_before;

        opts_before.limit(number_to_request);

        //sort by _id so messages are always in a consistent order even with matching timestamps
        std::string query_operation;
        switch (feedback_next_previous) {
            case ALL_FEEDBACK_TYPE:
            case NEXT_FEEDBACK_EXCLUSIVE_TYPE:
                query_operation = "$gt";
                opts_before.sort(
                        document{}
                                << feedback_timestamp_field_name << 1
                                << "_id" << 1
                        << finalize
                );
                break;
            case PREVIOUS_FEEDBACK_EXCLUSIVE_REQUEST:
                query_operation = "$lt";
                opts_before.sort(
                        document{}
                                << feedback_timestamp_field_name << -1
                                << "_id" << -1
                        << finalize
                );
                break;
            case PREVIOUS_FEEDBACK_INCLUSIVE_REQUEST:
                query_operation = "$lte";
                opts_before.sort(
                        document{}
                                << feedback_timestamp_field_name << -1
                                << "_id" << -1
                        << finalize
                );
                break;
        }

        //find documents
        feedback_cursor = match_results_collection.find(
                document{}
                        << feedback_timestamp_field_name << open_document
                                << query_operation << bsoncxx::types::b_date{last_time_feedback_extracted}
                        << close_document
                 << finalize,
                opts_before
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Exception while running database query inside extract_and_save_feedback_to_response.cpp.");
        return false;
    }

    if (!feedback_cursor) {
        const std::string error_string = "Cursor optional value was not set, this should never happen.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("'Previous' cursor was not properly set after database query.");
        return false;
    }

    std::vector<handle_feedback::FeedbackElement*> feedback_vector;
    for (const auto& feedback_doc : *feedback_cursor) {
        if (!saveFeedbackInfo(
                feedback_doc,
                feedback_collection_name,
                feedback_type,
                feedback_timestamp_field_name,
                feedback_message_field_name,
                feedback_sent_by_user_oid,
                feedback_marked_as_spam_by_admin_name_field_name,
                feedback_vector)
        ) {
            for(auto& x : feedback_vector) {
                delete x;
            }
            //error is already stored
            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return false;
        }
    }

    switch (feedback_next_previous) {
        case NEXT_FEEDBACK_EXCLUSIVE_TYPE:
            break;
        case PREVIOUS_FEEDBACK_EXCLUSIVE_REQUEST:
        case PREVIOUS_FEEDBACK_INCLUSIVE_REQUEST:
            //sort vector because previous feedback is returned in descending order
            std::sort(feedback_vector.begin(), feedback_vector.end(),
                [](const handle_feedback::FeedbackElement* lhs, handle_feedback::FeedbackElement* rhs) {
                    return lhs->single_feedback_unit().timestamp_stored_on_server() <
                           rhs->single_feedback_unit().timestamp_stored_on_server();
                }
            );
            [[fallthrough]];
        case ALL_FEEDBACK_TYPE:
            //this needs to be added to 'all feedback type' as well as previous requests
            addCapToResponse(response, number_to_request, feedback_vector);
            break;
    }

    for (auto& feedback : feedback_vector) {
        response->mutable_feedback_list()->Add()->Swap(feedback);
    }

    if (feedback_next_previous == NextPreviousFeedback::NEXT_FEEDBACK_EXCLUSIVE_TYPE
        || feedback_next_previous == NextPreviousFeedback::ALL_FEEDBACK_TYPE) {
        addCapToResponse(response, number_to_request, feedback_vector);
    }

    //Used for testing
    /*std::cout << "Feedback vector\n[";
    for(const auto& feedback : response->feedback_list()) {
        std::cout << "\nCase: " << feedback.TypeOfElement_case();

        if(feedback.has_single_feedback_unit()) {
            if(feedback_type == FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION) {
                std::cout << "\nActivity Name: " + feedback.single_feedback_unit().activity_name();
            }

            std::cout << "\n_id: " + feedback.single_feedback_unit().feedback_oid();
            std::cout << "\nMessage: " + feedback.single_feedback_unit().message();
            std::cout << "\nTimestamp: " + std::to_string(feedback.single_feedback_unit().timestamp_stored_on_server());
            std::cout << "\nmarked_as_spam_by_admin_name: " + feedback.single_feedback_unit().marked_as_spam_by_admin_name();
        }

        std::cout << ",\n";
    }
    std::cout << "]\n";*/

    return true;
}

void addCapToResponse(
        handle_feedback::GetFeedbackResponse* response,
        const int number_to_request,
        const std::vector<handle_feedback::FeedbackElement*>& feedback_vector
) {
    //these work like a 'cap' for the beginning of the list
    if ((int)feedback_vector.size() != number_to_request) {
        //add a message signaling there are no more feedback in the database to view
        response->mutable_feedback_list()->Add()->mutable_end_of_feedback_element();
    } else {
        //add a message signaling there are (probably) more feedback in the database to view
        response->mutable_feedback_list()->Add()->mutable_more_feedback_elements_available();
    }
}
