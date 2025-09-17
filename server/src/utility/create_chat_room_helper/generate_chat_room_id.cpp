//
// Created by jeremiah on 4/10/21.
//

#include "create_chat_room_helper.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_info_keys.h"
#include "chat_room_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


bool generateChatRoomId(mongocxx::database& chatRoomDB, std::string& chatRoomGeneratedId) {

    mongocxx::collection chatRoomNumberCollection = chatRoomDB[collection_names::CHAT_ROOM_INFO];

    //generating chat room Id and storing inside chatRoomGeneratedId
    while (chatRoomGeneratedId.size() < chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS - 1)
    { //run until a chat room id of size CHAT_ROOM_ID_NUMBER_OF_DIGITS or CHAT_ROOM_ID_NUMBER_OF_DIGITS - 1 is generated

        std::optional<std::string> findAndUpdateChatRoomInfoExceptionString;
        bsoncxx::stdx::optional<bsoncxx::document::value> findAndUpdateChatRoomInfoDoc;
        try {

            mongocxx::options::find_one_and_update opts;

            opts.projection(
                document{}
                    << "_id" << 0
                    << chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER << 1
                << finalize
            );

            //extract the numeric value representing the next chat room
            findAndUpdateChatRoomInfoDoc = chatRoomNumberCollection.find_one_and_update(
                document{}
                    << "_id" << chat_room_info_keys::ID
                    << finalize,
                document{}
                    << "$inc" << open_document
                    << chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER << bsoncxx::types::b_int64{1}
                    << close_document
                    << finalize,
                opts
            );
        }
        catch (mongocxx::logic_error& e) {
            findAndUpdateChatRoomInfoExceptionString = std::string(e.what());
        }

        if (findAndUpdateChatRoomInfoDoc) { //if chat room info was found

            bsoncxx::document::view chatRoomInfoDoc = findAndUpdateChatRoomInfoDoc->view();

            //NOTE: this is an unsigned 64 bit int and mongoDB does not have this type, however
            // casting a signed val to an unsigned val does not lose any data
            unsigned long long chatRoomNumber;

            auto chatRoomNumberElement = chatRoomInfoDoc[chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER];
            if (chatRoomNumberElement &&
                chatRoomNumberElement.type() == bsoncxx::type::k_int64) { //if element exists and is type bool
                chatRoomNumber = (unsigned long long) chatRoomNumberElement.get_int64().value;
            } else { //if element does not exist or is not type bool
                logElementError(__LINE__, __FILE__, chatRoomNumberElement,
                                chatRoomInfoDoc, bsoncxx::type::k_int64,
                                chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_INFO);

                return false;
            }

            chatRoomGeneratedId.clear();

            try {

                auto mangled = (chatRoomNumber * chat_room_values::CHAT_ROOM_ID_GENERATOR_PRIME) % chat_room_values::CHAT_ROOM_ID_GENERATOR_CONSTANT;

                while (mangled > 0) {
                    chatRoomGeneratedId += chat_room_values::CHAT_ROOM_ID_CHAR_LIST[mangled % chat_room_values::CHAT_ROOM_ID_CHAR_LIST.size()];
                    mangled = mangled / chat_room_values::CHAT_ROOM_ID_CHAR_LIST.size();
                }
            }
            catch (std::exception& e) {

                std::string errorString = "Exception when generating chatRoomAccountId.";

                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              findAndUpdateChatRoomInfoExceptionString, errorString,
                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                              "collection", collection_names::CHAT_ROOM_INFO,
                                              "Number_used", std::to_string(chatRoomNumber));

                return false;
            }
        } else { //if chat room info was not found
            std::string errorString = "Chat room info (which should always exist) was not found.";

            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          findAndUpdateChatRoomInfoExceptionString, errorString,
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", collection_names::CHAT_ROOM_INFO);

            return false;
        }
    }

    return true;
}