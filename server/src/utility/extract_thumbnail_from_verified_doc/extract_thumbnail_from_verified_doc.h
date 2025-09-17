//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

struct ExtractThumbnailAdditionalCommands {

public:
    enum ThumbnailAdditionCommandsEnum {
        THUMB_ADDITIONAL_DO_NOTHING, //Will extract the thumbnail and do nothing extra.
        THUMB_ADDITIONAL_ADD, //Will add a chat room id to the user picture document the thumbnail is extracted from.
        THUMB_ADDITIONAL_ADD_LIST, //Will add a LIST of chat room ids to the user picture document the thumbnail is extracted from.
        THUMB_ADDITIONAL_UPDATE, //Will add a chat room id to the user picture document AND remove a previous thumbnail_reference_oid if a different thumbnail was found.
        };

private:
    ThumbnailAdditionCommandsEnum command = ThumbnailAdditionCommandsEnum::THUMB_ADDITIONAL_DO_NOTHING;
    std::string chat_room_id;
    //this is expected to be empty OR a valid bsoncxx oid
    std::string thumbnail_reference_oid;
    bsoncxx::array::view chat_room_id_list;

    bool only_request_thumbnail_size = false;

public:

    ExtractThumbnailAdditionalCommands() = default;

    void setupForUpdate(const std::string& _chat_room_id, const std::string& _current_thumbnail_reference_oid) {
        command = ThumbnailAdditionCommandsEnum::THUMB_ADDITIONAL_UPDATE;
        chat_room_id = _chat_room_id;
        thumbnail_reference_oid = _current_thumbnail_reference_oid;
    }

    void setupForAddToSet(const std::string& _chat_room_id) {
        command = ThumbnailAdditionCommandsEnum::THUMB_ADDITIONAL_ADD;
        chat_room_id = _chat_room_id;
    }

    void setupForAddArrayValuesToSet(const bsoncxx::array::view& _chat_room_id_list) {
        command = ThumbnailAdditionCommandsEnum::THUMB_ADDITIONAL_ADD_LIST;
        chat_room_id_list = _chat_room_id_list;
    }

    void setOnlyRequestThumbnailSize() {
        only_request_thumbnail_size = true;
    }

    bool getOnlyRequestThumbnailSize() const {
        return only_request_thumbnail_size;
    }

    [[nodiscard]] ThumbnailAdditionCommandsEnum getCommand() const {
        return command;
    }

    [[nodiscard]] std::string getChatRoomId() const {
        return chat_room_id;
    }

    [[nodiscard]] bsoncxx::array::view getChatRoomIdsList() const {
        return chat_room_id_list;
    }

    [[nodiscard]] std::string getThumbnailReferenceOid() const {
        return thumbnail_reference_oid;
    }
};

//extracts thumbnail from first element of 'PICTURES' array and sets it to passed thumbnail string
//returns false if an error occurs true if no error occurs; stores all errors
//requires PICTURES to be projected to userAccountDocView
bool extractThumbnailFromUserAccountDoc(
        mongocxx::database& accountsDB,
        const bsoncxx::document::view& userAccountDocView,
        const bsoncxx::oid& userAccountOID,
        mongocxx::client_session* session,
        const std::function<void(
                std::string& /*thumbnail*/,
                const int /*thumbnail_size*/,
                const std::string& /*thumbnail_reference_oid*/,
                const int /*index*/,
                const std::chrono::milliseconds& /*timestamp*/
        )>& moveThumbnailToResult,
        const ExtractThumbnailAdditionalCommands& additionalCommands = ExtractThumbnailAdditionalCommands()
);
