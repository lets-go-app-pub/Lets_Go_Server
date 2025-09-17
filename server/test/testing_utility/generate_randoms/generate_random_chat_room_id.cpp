//
// Created by jeremiah on 6/1/22.
//

#include <chat_room_values.h>
#include "generate_randoms.h"

std::string generateRandomChatRoomId(int num_chars) {
    std::string chat_room_id;

    for(int i = 0; i < num_chars; i++) {
        int value = rand() % chat_room_values::CHAT_ROOM_ID_CHAR_LIST.size();
        chat_room_id += chat_room_values::CHAT_ROOM_ID_CHAR_LIST[value];
    }

    return chat_room_id;
}