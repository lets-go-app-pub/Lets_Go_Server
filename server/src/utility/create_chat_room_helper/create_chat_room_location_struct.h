//
// Created by jeremiah on 3/8/23.
//

#pragma once

struct CreateChatRoomLocationStruct {
    const double longitude;
    const double latitude;

    CreateChatRoomLocationStruct() = delete;

    CreateChatRoomLocationStruct(
            double _longitude,
            double _latitude
    ) :
            longitude(_longitude),
            latitude(_latitude) {}
};