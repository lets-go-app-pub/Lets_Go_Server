//
// Created by jeremiah on 3/8/23.
//

#pragma once

#include <bsoncxx/oid.hpp>
#include <chrono>

#include "chat_room_values.h"
#include "server_parameter_restrictions.h"

//This object will take control of data pointed to by qr_code and handle deallocation.
struct FullEventValues {
    const bsoncxx::oid event_oid;
    std::string* qr_code = nullptr;
    const std::string qr_code_message;
    const std::chrono::milliseconds qr_code_time_updated{};

    const int min_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;

    FullEventValues() = delete;

    FullEventValues(const FullEventValues& copy) = delete;

    FullEventValues(
            FullEventValues&& other
    ) noexcept:
            event_oid(other.event_oid),
            qr_code_message(other.qr_code_message),
            qr_code_time_updated(other.qr_code_time_updated),
            min_age(other.min_age) {
        qr_code = other.qr_code;
        other.qr_code = nullptr; //Ok to call delete on nullptr
    }

    FullEventValues(
            const bsoncxx::oid& _event_oid,
            std::string* _qr_code,
            const std::string& _qr_code_message,
            const std::chrono::milliseconds& _qr_code_time_updated,
            int _min_age
    ) :
            event_oid(_event_oid),
            qr_code(_qr_code),
            qr_code_message(_qr_code_message.empty() ? chat_room_values::QR_CODE_MESSAGE_DEFAULT : _qr_code_message),
            qr_code_time_updated(_qr_code_time_updated),
            min_age(_min_age) {}

    explicit FullEventValues(
            const bsoncxx::oid& _event_oid,
            int _min_age
    ) :
            event_oid(_event_oid),
            qr_code(new std::string(chat_room_values::QR_CODE_DEFAULT)),
            qr_code_message(chat_room_values::QR_CODE_MESSAGE_DEFAULT),
            qr_code_time_updated(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT),
            min_age(_min_age) {}

    ~FullEventValues() {
        delete qr_code;
    }
};