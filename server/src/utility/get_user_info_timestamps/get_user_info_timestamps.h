//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <mongocxx/collection.hpp>

#include <PreLoginTimestamps.grpc.pb.h>

//saves the saved timestamps of personal info to the passed response
//it will also return true if more info is needed and false if more info is not needed
//accepts types for loginfunction::LoginResponse and loginSupport::NeededVeriInfoResponse
bool getUserInfoTimestamps(const bsoncxx::document::view& user_account_doc_view,
                           PreLoginTimestampsMessage* pre_login_timestamps,
                           google::protobuf::RepeatedField<google::protobuf::int64>* picture_timestamps);
