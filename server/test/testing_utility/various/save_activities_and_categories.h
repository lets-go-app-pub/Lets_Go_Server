//
// Created by jeremiah on 10/4/22.
//

#pragma once

#include <mongocxx/database.hpp>
#include <google/protobuf/repeated_ptr_field.h>
#include "RequestMessages.grpc.pb.h"

void saveActivitiesAndCategories(
        mongocxx::database& accounts_db,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* mutable_categories,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* mutable_activities
);