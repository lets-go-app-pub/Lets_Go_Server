//
// Created by jeremiah on 3/18/21.
//
#pragma once

#include <bsoncxx/json.hpp>

#include <mongocxx/collection.hpp>

#include <RequestFields.grpc.pb.h>
#include <MemberSharedInfoMessage.grpc.pb.h>

//helper function for requestBirthday
bool requestBirthdayHelper(const bsoncxx::document::view& userAccountDocView, BirthdayMessage* response);

//helper function for requestEmail
bool requestEmailHelper(const bsoncxx::document::view& userAccountDocView, EmailMessage* response);

//helper function for requestGender
bool requestGenderHelper(const bsoncxx::document::view& userAccountDocView, std::string* response);

//helper function for requestName
bool requestNameHelper(const bsoncxx::document::view& userAccountDocView, std::string* response);

//helper function for RequestPostLoginInfoHelper
bool requestPostLoginInfoHelper(const bsoncxx::document::view& userAccountDocView, PostLoginMessage* response);

//helper function for requestCategories
//returns 1 if successful, 0 if failed and 2 if current_object_oid not found
bool requestCategoriesHelper(const bsoncxx::document::view& userAccountDocView,
                             google::protobuf::RepeatedPtrField<CategoryActivityMessage>* activityArray);

bool requestOutOfDateIconIndexValues(const google::protobuf::RepeatedField<google::protobuf::int64>& iconTimestamps,
                                     google::protobuf::RepeatedField<google::protobuf::int64>* indexToBeUpdated,
                                     mongocxx::database& accountsDB);

//helper function for requestServerActivities sends all activities and categories to the client
bool requestAllServerCategoriesActivitiesHelper(
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* grpcCategoriesArray,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* grpcActivitiesArray,
        mongocxx::database& accountsDB);

//helper function for requestPicture; sends back pictures requested by picture index values inside requestedIndexes
//NOTE: Order relative to requestedIndexes is NOT guaranteed for the pictures being returned.
bool requestPicturesHelper(std::set<int>& requestedIndexes, mongocxx::database& accountsDB,
                           const bsoncxx::oid& userAccountOID, const bsoncxx::document::view& userAccountDocView,
                           mongocxx::client& mongoCppClient, mongocxx::collection& userAccountsCollection,
                           const std::chrono::milliseconds& currentTimestamp,
                           const std::function<void(int)>& setPictureEmptyResponse,
                           const std::function<void(const bsoncxx::oid& picture_oid, std::string&& pictureByteString,
                                                    int pictureSize,
                                                    int indexNumber,
                                                    const std::chrono::milliseconds& picture_timestamp,
                                                    bool extracted_from_deleted_pictures,
                                                    bool references_removed_after_delete
                                                    )
                           >& setPictureToResponse,
                           mongocxx::client_session* session,
                           mongocxx::database* deletedDB = nullptr //If searching deleted pictures is not wanted; set to null.
                           );


