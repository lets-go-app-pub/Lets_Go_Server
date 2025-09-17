//
// Created by jeremiah on 3/19/21.
//

#include <optional>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include <ReportMessages.grpc.pb.h>

//this completes the "move" from the user 'extracted' list to the appropriate list of the match completes the match
//it will also do some error checking
bool moveExtractedElementToOtherAccount(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::optional<bsoncxx::document::view>& user_extracted_array_document,
        const bsoncxx::document::view& user_account_document_view,
        const bsoncxx::oid& match_account_oid,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp
#ifdef LG_TESTING
        , int& testing_result
#endif // TESTING
);

//adds the user to the event and send through the signal to return a message in the chat stream to join the event
bool handleUserSwipedYesOnEvent(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& chat_room_collection,
        mongocxx::client_session* session,
        const bsoncxx::document::view& user_account_document_view,
        const std::string& chat_room_id,
        const bsoncxx::oid& event_account_oid,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp
#ifdef LG_TESTING
        , int& testing_result
#endif // TESTING
);

//adds to the projectReducedArraysDocument builder to $pull all elements from the array arrayListKey with arrayListOIDKey matching matchOID
void addPullArrayToQuery(
        const bsoncxx::oid& match_oid,
        const bsoncxx::document::view& document_view,
        bsoncxx::builder::stream::document& project_reduced_arrays_doc_builder,
        const std::string& array_list_key,
        const std::string& array_list_oid_key,
        int number_elements
);

//appends to the projectReducedArraysDocument for use in an aggregation pipeline 'project' stage
//appends a function that filters the array(arrayName) and only returns the values where the value of arrayOIDElementName is matchOID
void addFilterForProjectionStage(
        bsoncxx::builder::stream::document& project_reduced_arrays_doc_builder,
        const bsoncxx::oid& match_oid,
        const std::string& array_name,
        const std::string& array_oid_element_name
);

//will extract the element from the 'extracted' list if it exists and save it
//will error check 'other user said yes' list and 'algorithm matches' list and if a duplicate element exists, it will remove it
//will then update 'previously matched accounts' list
//user_account_doc & user_account_doc_view will be set to the user's extracted document
//user_extracted_array_doc will be set to the element from the 'extracted' list if it was found
//statistics_oid will be set to the document statistics oid if the 'extracted' list element was found
//returns false if an error occurred, true if no error occurred; NOTE: A return of 'true' does not guarantee any values are set.
bool handleUserExtractedAccount(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::collection& user_account_collection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& match_account_oid,
        const ResponseType& response_type,
        std::optional<bsoncxx::document::value>& user_account_doc,
        bsoncxx::document::view& user_account_doc_view,
        std::optional<bsoncxx::document::view>& user_extracted_array_doc,
        std::optional<bsoncxx::oid>& statistics_oid
);

//updates the match results to the statistics document
void updateMatchStatisticsDocument(
        const mongocxx::client& mongo_cpp_client,
        const std::chrono::milliseconds& current_timestamp,
        const std::optional<bsoncxx::oid>& match_statistics_doc_oid,
        ResponseType response_type
);

