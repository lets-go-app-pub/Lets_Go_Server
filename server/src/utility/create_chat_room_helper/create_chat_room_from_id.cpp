//
// Created by jeremiah on 4/10/21.
//

#include <random>
#include "create_chat_room_helper.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "chat_room_values.h"
#include "chat_room_header_keys.h"
#include "chat_room_message_keys.h"
#include "generate_chat_room_cap_message.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool createChatRoomFromId(
        const bsoncxx::array::view& header_accounts_in_chat_room_array,
        const std::string& chat_room_name,
        std::string& cap_message_uuid,
        const bsoncxx::oid& chat_room_created_by_oid,
        const GenerateNewChatRoomTimes& chat_room_times,
        mongocxx::client_session* session,
        const std::function<void(const std::string&, const std::string&)>& set_return_status,
        const bsoncxx::array::view& matching_array,
        const mongocxx::database& chat_room_db,
        const std::string& chat_room_id,
        const std::optional<CreateChatRoomLocationStruct>& location_values,
        const std::optional<FullEventValues>& event_values
) {

    if(session == nullptr) {
        const std::string error_string = "createChatRoomFromId() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "chatRoomId", chat_room_id,
                "chatRoomName", chat_room_name
        );
        return false;
    }

    std::string chat_room_generated_password;

    std::string random_seed_str = bsoncxx::oid{}.to_string();
    std::seed_seq seed(random_seed_str.begin(),random_seed_str.end());
    std::mt19937 rng(seed);

    //this must be size-2 it is a C style string and so the final char will be null terminator
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, chat_room_values::CHAT_ROOM_PASSWORD_CHAR_LIST.size() - 1);

    //generating chat room password and storing inside chatRoomGeneratedPassword
    for (size_t i = 0; i < chat_room_values::CHAT_ROOM_PASSWORD_NUMBER_OF_DIGITS; i++) {
        chat_room_generated_password += chat_room_values::CHAT_ROOM_PASSWORD_CHAR_LIST[distribution(rng)];
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    //upsert new chat room header
    std::optional<std::string> insert_chat_room_header_exception_string;
    mongocxx::stdx::optional<mongocxx::result::insert_many> insert_chat_room_header_result;
    try {

        //NOTE: The index must be created on the empty collection BEFORE the document is inserted when inside
        // a transaction.
        // mongocxx::options::index::background(true) should NOT be set here because the index must be created
        // immediately.
        chat_room_collection.create_index(
            *session,
            document{}
                << chat_room_shared_keys::TIMESTAMP_CREATED << 1 //time stored (not unique)
            << finalize
        );

        //Make a TTL index where the document is deleted at the time specified by the field.
        mongocxx::options::index pending_expiration_index;
        pending_expiration_index.expire_after(std::chrono::seconds{0});

        chat_room_collection.create_index(
                *session,
                document{}
                    << chat_room_message_keys::TIMESTAMP_TEMPORARY_EXPIRES_AT << 1
                << finalize,
                pending_expiration_index
        );

        const auto chat_room_created_time_date = bsoncxx::types::b_date{chat_room_times.chat_room_created_time};
        const auto chat_room_last_active_time_date = bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time};

        bsoncxx::builder::stream::document header_document;

        header_document
                << "_id" << chat_room_header_keys::ID
                << chat_room_header_keys::CHAT_ROOM_NAME << bsoncxx::types::b_string{chat_room_name}
                << chat_room_header_keys::CHAT_ROOM_PASSWORD << bsoncxx::types::b_string{chat_room_generated_password}
                << chat_room_shared_keys::TIMESTAMP_CREATED << chat_room_created_time_date
                << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << chat_room_last_active_time_date
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << header_accounts_in_chat_room_array;

        if (matching_array.empty()) { //if the matching array was empty
            header_document
                    << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{};
        } else { //if the matching array was not empty
            header_document
                    << chat_room_header_keys::MATCHING_OID_STRINGS << matching_array;
        }

        if(event_values) {
            header_document
                    << chat_room_header_keys::EVENT_ID << event_values->event_oid
                    << chat_room_header_keys::QR_CODE << std::move(*event_values->qr_code)
                    << chat_room_header_keys::QR_CODE_MESSAGE << event_values->qr_code_message
                    << chat_room_header_keys::QR_CODE_TIME_UPDATED << bsoncxx::types::b_date{event_values->qr_code_time_updated}
                    << chat_room_header_keys::MIN_AGE << event_values->min_age;
        }

        if(location_values) {
            header_document
                << chat_room_header_keys::PINNED_LOCATION << open_document
                        << chat_room_header_keys::pinned_location::LONGITUDE << location_values->longitude
                        << chat_room_header_keys::pinned_location::LATITUDE << location_values->latitude
                << close_document;
        }

        std::vector<bsoncxx::document::value> documents;

        documents.emplace_back(header_document << finalize);

        cap_message_uuid = generateUUID();

        documents.emplace_back(
                generateChatRoomCapMessage(
                        cap_message_uuid,
                        chat_room_created_by_oid,
                        chat_room_times.chat_room_cap_message_time
                )
        );

        //NOTE: Is the order the documents are inserted guaranteed? Meaning will the header be inserted before the
        // cap document is inserted? If not it is possible that the cap will never be sent back. Also does the change
        // stream always stream them back in the order they were inserted? However, for now (and maybe forever) these
        // questions are irrelevant.
        insert_chat_room_header_result = chat_room_collection.insert_many(
                *session,
                documents
        );
    }
    catch (const mongocxx::logic_error& e) {
        insert_chat_room_header_exception_string = std::string(e.what());
    }

    if (insert_chat_room_header_result && insert_chat_room_header_result->result().inserted_count() == 2) { //if upsert was successful
        set_return_status(chat_room_id, chat_room_generated_password);
    } else { //if upsert failed
        //NOTE: this will also be sent if the chat room already existed (it will be stored in the error string
        const std::string error_string = "Inserting new chat room header failed OR generating index failed.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                insert_chat_room_header_exception_string, error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_INFO,
                "chat_room_id", chat_room_id,
                "chat_room_pass", chat_room_generated_password,
                "inserted_count", std::to_string(insert_chat_room_header_result->result().inserted_count())
        );
        return false;
    }

    return true;
}