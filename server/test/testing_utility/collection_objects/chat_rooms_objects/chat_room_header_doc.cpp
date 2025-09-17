//
// Created by jeremiah on 6/1/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>

#include "chat_rooms_objects.h"
#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this ChatRoomHeaderDoc object to a document and saves it to the passed builder
void ChatRoomHeaderDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result
        ) const {

    document_result
            << "_id" << chat_room_header_keys::ID
            << chat_room_shared_keys::TIMESTAMP_CREATED << shared_properties.timestamp
            << chat_room_header_keys::CHAT_ROOM_NAME << chat_room_name
            << chat_room_header_keys::CHAT_ROOM_PASSWORD << chat_room_password
            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << chat_room_last_active_time;

    if (matching_oid_strings == nullptr) {
        document_result
                << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{};
    } else {
        bsoncxx::builder::basic::array matching_oid_strings_arr;

        for (const std::string& oid : *matching_oid_strings) {
            matching_oid_strings_arr.append(oid);
        }

        document_result
                << chat_room_header_keys::MATCHING_OID_STRINGS << matching_oid_strings_arr;
    }

    if(event_id) {
        document_result
                << chat_room_header_keys::EVENT_ID << *event_id;
    }

    if(qr_code) {
        document_result
                << chat_room_header_keys::QR_CODE << *qr_code;
    }

    if(qr_code_message) {
        document_result
                << chat_room_header_keys::QR_CODE_MESSAGE << *qr_code_message;
    }

    if(qr_code_time_updated) {
        document_result
                << chat_room_header_keys::QR_CODE_TIME_UPDATED << *qr_code_time_updated;
    }

    if(pinned_location) {
        document_result
                << chat_room_header_keys::PINNED_LOCATION << open_document
                    << chat_room_header_keys::pinned_location::LONGITUDE << pinned_location->longitude
                    << chat_room_header_keys::pinned_location::LATITUDE << pinned_location->latitude
                << close_document;
    }

    if(min_age) {
        document_result
                << chat_room_header_keys::MIN_AGE << *min_age;
    }

    bsoncxx::builder::basic::array accounts_in_chat_room_arr;

    for (const AccountsInChatRoom& account : accounts_in_chat_room) {

        bsoncxx::builder::stream::document accounts_in_chat_room_doc;

        account.convertToDocument(accounts_in_chat_room_doc);

        accounts_in_chat_room_arr.append(accounts_in_chat_room_doc.view());
    }

    document_result
        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << accounts_in_chat_room_arr;

}

bool ChatRoomHeaderDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {
        mongocxx::options::update updateOptions;
        updateOptions.upsert(true);

        chat_room_collection.update_one(
                document{}
                        << "_id" << chat_room_header_keys::ID
                        << finalize,
                document{}
                        << "$set" << insertDocument.view()
                        << finalize,
                updateOptions);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

void ChatRoomHeaderDoc::generateHeaderForEventChatRoom(
        const std::string& _chat_room_id,
        const std::string& _chat_room_name,
        const std::string& _chat_room_password,
        const std::chrono::milliseconds& _chat_room_last_active_time,
        const bsoncxx::oid _event_oid,
        const std::string& _qr_code,
        const std::string& _qr_code_message,
        const std::chrono::milliseconds & _qr_code_time_updated,
        const double longitude,
        const double latitude,
        const int _min_age,
        const std::chrono::milliseconds& _created_time,
        const bsoncxx::oid& account_oid
        ) {

    const bsoncxx::types::b_date bsoncxx_chat_room_last_active_time{_chat_room_last_active_time};

    chat_room_id = _chat_room_id;
    chat_room_name = _chat_room_name;
    chat_room_password = _chat_room_password;
    chat_room_last_active_time = bsoncxx_chat_room_last_active_time;
    matching_oid_strings = nullptr;

    event_id = _event_oid;
    qr_code = _qr_code;
    qr_code_message = _qr_code_message;
    qr_code_time_updated = bsoncxx::types::b_date{_qr_code_time_updated};

    pinned_location = PinnedLocation{
            longitude,
            latitude
    };

    min_age = _min_age;

    shared_properties.timestamp = bsoncxx::types::b_date{_created_time};

    UserAccountDoc user_account_doc(account_oid);

    bsoncxx::oid thumbnail_reference;
    int thumbnail_size = 0;
    for(const auto& pic : user_account_doc.pictures) {
        if(pic.pictureStored()) {
            bsoncxx::types::b_date thumbnail_timestamp{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    thumbnail_reference,
                    thumbnail_timestamp
            );

            UserPictureDoc user_pic_doc(thumbnail_reference);
            thumbnail_size = user_pic_doc.thumbnail_size_in_bytes;
            break;
        }
    }

    accounts_in_chat_room.clear();
    accounts_in_chat_room.emplace_back(
            AccountsInChatRoom{
                    account_oid,
                    AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
                    user_account_doc.first_name,
                    thumbnail_reference.to_string(),
                    bsoncxx_chat_room_last_active_time,
                    thumbnail_size,
                    bsoncxx_chat_room_last_active_time,
                    std::vector<bsoncxx::types::b_date>{
                            bsoncxx_chat_room_last_active_time
                    }
            }
    );
}

bool ChatRoomHeaderDoc::getFromCollection(const std::string& _chat_room_id) {
    chat_room_id = _chat_room_id;
    return getFromCollection(bsoncxx::oid{});
}

bool ChatRoomHeaderDoc::getFromCollection(const bsoncxx::oid&) {

    assert(!chat_room_id.empty());

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = chat_room_collection.find_one(document{}
                                                                << "_id" << chat_room_header_keys::ID
                                                                << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal, chat_room_id);
}

bool ChatRoomHeaderDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val,
                                           const std::string& updated_chat_room_id) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view, updated_chat_room_id);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool ChatRoomHeaderDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document,
                                               const std::string& updated_chat_room_id) {

    chat_room_id = updated_chat_room_id;
    try {

        shared_properties =
                ChatRoomShared(
                        extractFromBsoncxx_k_date(
                                user_account_document,
                                chat_room_shared_keys::TIMESTAMP_CREATED
                        )
                );

        chat_room_name = extractFromBsoncxx_k_utf8(
                user_account_document,
                chat_room_header_keys::CHAT_ROOM_NAME);

        chat_room_password = extractFromBsoncxx_k_utf8(
                user_account_document,
                chat_room_header_keys::CHAT_ROOM_PASSWORD);

        chat_room_last_active_time = extractFromBsoncxx_k_date(
                user_account_document,
                chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME);

        const auto matching_oid_strings_element = user_account_document[chat_room_header_keys::MATCHING_OID_STRINGS];

        if (matching_oid_strings_element.type() == bsoncxx::type::k_null) {
            matching_oid_strings = nullptr;
        } else {

            matching_oid_strings = std::make_unique<std::vector<std::string>>();
            bsoncxx::array::view matching_oid_strings_arr = extractFromBsoncxx_k_array(
                    user_account_document,
                    chat_room_header_keys::MATCHING_OID_STRINGS);

            for (const auto& ele : matching_oid_strings_arr) {
                matching_oid_strings->emplace_back(ele.get_string().value.to_string());
            }
        }

        const auto event_id_element = user_account_document[chat_room_header_keys::EVENT_ID];
        event_id.reset();
        if(event_id_element) {
            event_id = event_id_element.get_oid().value;
        }

        const auto qr_code_element = user_account_document[chat_room_header_keys::QR_CODE];
        qr_code.reset();
        if(qr_code_element) {
            qr_code = qr_code_element.get_string().value.to_string();
        }

        const auto qr_code_message_element = user_account_document[chat_room_header_keys::QR_CODE_MESSAGE];
        qr_code_message.reset();
        if(qr_code_message_element) {
            qr_code_message = qr_code_message_element.get_string().value.to_string();
        }

        const auto qr_code_time_updated_element = user_account_document[chat_room_header_keys::QR_CODE_TIME_UPDATED];
        qr_code_time_updated.reset();
        if(qr_code_time_updated_element) {
            qr_code_time_updated = qr_code_time_updated_element.get_date();
        }

        const auto pinned_location_element = user_account_document[chat_room_header_keys::PINNED_LOCATION];
        pinned_location.reset();
        if(pinned_location_element) {
            const bsoncxx::document::view pinned_location_doc = pinned_location_element.get_document().value;

            pinned_location = PinnedLocation(
                pinned_location_doc[chat_room_header_keys::pinned_location::LONGITUDE].get_double().value,
                pinned_location_doc[chat_room_header_keys::pinned_location::LATITUDE].get_double().value
            );
        }

        const auto min_age_element = user_account_document[chat_room_header_keys::MIN_AGE];
        min_age.reset();
        if(min_age_element) {
            min_age = min_age_element.get_int32().value;
        }

        bsoncxx::array::view accounts_in_chat_room_arr = extractFromBsoncxx_k_array(
                user_account_document,
                chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM);

        accounts_in_chat_room.clear();
        for (const auto& ele : accounts_in_chat_room_arr) {
            bsoncxx::document::view account_state_doc = ele.get_document().value;

            std::vector<bsoncxx::types::b_date> temp_times_joined_left;

            bsoncxx::array::view times_joined_left_arr = account_state_doc[chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT].get_array().value;

            for (const auto& time_joined : times_joined_left_arr) {
                temp_times_joined_left.emplace_back(time_joined.get_date().value);
            }

            accounts_in_chat_room.emplace_back(
                    AccountsInChatRoom(
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID].get_oid().value,
                            AccountStateInChatRoom(
                                    account_state_doc[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM].get_int32().value),
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::FIRST_NAME].get_string().value.to_string(),
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE].get_string().value.to_string(),
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP].get_date(),
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE].get_int32().value,
                            account_state_doc[chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME].get_date(),
                            temp_times_joined_left
                    )
            );
        }

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "ErrorExtractingFromBsoncxx EXCEPTION THROWN\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ChatRoomHeaderDoc::operator==(const ChatRoomHeaderDoc& other) const {
    bool return_value = true;

    checkForEquality(
            chat_room_id,
            other.chat_room_id,
            "CHAT_ROOM_ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            chat_room_name,
            other.chat_room_name,
            "CHAT_ROOM_NAME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            chat_room_password,
            other.chat_room_password,
            "CHAT_ROOM_PASSWORD",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            chat_room_last_active_time.value.count(),
            other.chat_room_last_active_time.value.count(),
            "CHAT_ROOM_LAST_ACTIVE_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (matching_oid_strings == nullptr
        || other.matching_oid_strings == nullptr) {
        checkForEquality(
                matching_oid_strings,
                other.matching_oid_strings,
                "MATCHING_OID_STRINGS",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        if ((*other.matching_oid_strings).size() == (*matching_oid_strings).size()) {
            for (int i = 0; i < (int)(*matching_oid_strings).size(); i++) {
                checkForEquality(
                        (*matching_oid_strings)[i],
                        (*other.matching_oid_strings)[i],
                        "MATCHING_OID_STRINGS.oid",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }
        } else {
            checkForEquality(
                    (*matching_oid_strings).size(),
                    (*other.matching_oid_strings).size(),
                    "MATCHING_OID_STRINGS.size()",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    }

    if(event_id && other.event_id) {
        checkForEquality(
                event_id->to_string(),
                other.event_id->to_string(),
                "EVENT_ID",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                event_id.operator bool(),
                other.event_id.operator bool(),
                "EVENT_ID existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if(qr_code && other.qr_code) {
        checkForEquality(
                *qr_code,
                *other.qr_code,
                "QR_CODE",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                qr_code.operator bool(),
                other.qr_code.operator bool(),
                "QR_CODE existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if(qr_code_message && other.qr_code_message) {
        checkForEquality(
                *qr_code_message,
                *other.qr_code_message,
                "QR_CODE_MESSAGE",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                qr_code_message.operator bool(),
                other.qr_code_message.operator bool(),
                "QR_CODE_MESSAGE existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if(qr_code_time_updated && other.qr_code_time_updated) {
        checkForEquality(
                *qr_code_time_updated,
                *other.qr_code_time_updated,
                "QR_CODE_TIME_UPDATED",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                qr_code_time_updated.operator bool(),
                other.qr_code_time_updated.operator bool(),
                "QR_CODE_TIME_UPDATED existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if(pinned_location && other.pinned_location) {
        checkForEquality(
                pinned_location->longitude,
                other.pinned_location->longitude,
                "PINNED_LOCATION.LONGITUDE",
                OBJECT_CLASS_NAME,
                return_value
        );

        checkForEquality(
                pinned_location->latitude,
                other.pinned_location->latitude,
                "PINNED_LOCATION.LATITUDE",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                pinned_location.operator bool(),
                other.pinned_location.operator bool(),
                "PINNED_LOCATION existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if(min_age && other.min_age) {
        checkForEquality(
                *min_age,
                *other.min_age,
                "MIN_AGE",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                min_age.operator bool(),
                other.min_age.operator bool(),
                "MIN_AGE existence",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.accounts_in_chat_room.size() == accounts_in_chat_room.size()) {
        for (int i = 0; i < (int)accounts_in_chat_room.size(); i++) {

            checkForEquality(
                    accounts_in_chat_room[i].account_oid.to_string(),
                    other.accounts_in_chat_room[i].account_oid.to_string(),
                    "ACCOUNTS_IN_CHAT_ROOM.account_oid",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].state_in_chat_room,
                    other.accounts_in_chat_room[i].state_in_chat_room,
                    "ACCOUNTS_IN_CHAT_ROOM.state_in_chat_room",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].first_name,
                    other.accounts_in_chat_room[i].first_name,
                    "ACCOUNTS_IN_CHAT_ROOM.first_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].thumbnail_reference,
                    other.accounts_in_chat_room[i].thumbnail_reference,
                    "ACCOUNTS_IN_CHAT_ROOM.thumbnail_reference",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].thumbnail_timestamp.value.count(),
                    other.accounts_in_chat_room[i].thumbnail_timestamp.value.count(),
                    "ACCOUNTS_IN_CHAT_ROOM.thumbnail_timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].thumbnail_size,
                    other.accounts_in_chat_room[i].thumbnail_size,
                    "ACCOUNTS_IN_CHAT_ROOM.thumbnail_size",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    accounts_in_chat_room[i].last_activity_time.value.count(),
                    other.accounts_in_chat_room[i].last_activity_time.value.count(),
                    "ACCOUNTS_IN_CHAT_ROOM.last_activity_time",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if (other.accounts_in_chat_room[i].times_joined_left.size() ==
                accounts_in_chat_room[i].times_joined_left.size()) {
                for (int j = 0; j < (int)accounts_in_chat_room[i].times_joined_left.size(); j++) {
                    checkForEquality(
                            accounts_in_chat_room[i].times_joined_left[j].value.count(),
                            other.accounts_in_chat_room[i].times_joined_left[j].value.count(),
                            "ACCOUNTS_IN_CHAT_ROOM.times_joined_left",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                }
            } else {
                checkForEquality(
                        accounts_in_chat_room[i].times_joined_left.size(),
                        other.accounts_in_chat_room[i].times_joined_left.size(),
                        "ACCOUNTS_IN_CHAT_ROOM.times_joined_left.size()",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }
        }
    } else {
        checkForEquality(
                accounts_in_chat_room.size(),
                other.accounts_in_chat_room.size(),
                "ACCOUNTS_IN_CHAT_ROOM.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            shared_properties.timestamp.value.count(),
            other.shared_properties.timestamp.value.count(),
            "SHARED_PROPERTIES.timestamp",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ChatRoomHeaderDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
