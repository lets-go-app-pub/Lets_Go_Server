//proto file is LoginSupportFunctions.proto

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <AccountState.grpc.pb.h>
#include <handle_function_operation_exception.h>
#include <store_info_to_user_statistics.h>
#include <set_fields_helper_functions/set_fields_helper_functions.h>

#include "login_support_functions.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "user_pictures_keys.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "chat_room_header_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "extract_data_from_bsoncxx.h"
#include "utility_testing_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void logoutFunctionMongoDbImplementation(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
);

void runAddMatchingAccount(
        const std::vector<std::string>& matching_users_by_chat_room,
        const std::vector<std::string>& other_users_matched_with,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        unsigned int matching_users_chat_room_index,
        bsoncxx::builder::basic::array& matching_accounts_to_add
);

void runRemoveMatchingAccount(
        const std::vector<std::string>& matching_users_by_chat_room,
        const std::vector<std::string>& other_users_matched_with,
        const bsoncxx::oid& user_account_oid,
        unsigned int other_users_matched_with_index,
        bsoncxx::builder::basic::array& matching_accounts_to_remove
);

//primary function for logout, called from gRPC server implementation
void logoutFunctionMongoDb(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
) {

    handleFunctionOperationException(
            [&] {
                logoutFunctionMongoDbImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request
    );
}

void logoutFunctionMongoDbImplementation(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    {
        ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(basic_info_return_status);
            return;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    //setting error as default
    response->set_return_status(ReturnStatus::UNKNOWN);
    response->set_timestamp(-1L);

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{user_account_oid_str};

    //update last observed time
    bsoncxx::builder::stream::document merge_document;
    merge_document
            << user_account_keys::LOGGED_IN_TOKEN << ""
            << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << bsoncxx::types::b_date{current_timestamp}
            << user_account_keys::LOGGED_IN_INSTALLATION_ID << ""
            << finalize;

    {
        //Clear all arrays on logout.
        bsoncxx::builder::basic::array empty_array;
        appendDocumentToClearMatchingInfoAggregation(
                merge_document,
                empty_array.view()
        );
    }

    //project verified pictures key to extract thumbnail
    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::STATUS << 1
            << user_account_keys::AGE << 1
            << user_account_keys::BIRTH_YEAR << 1
            << user_account_keys::BIRTH_MONTH << 1
            << user_account_keys::BIRTH_DAY_OF_MONTH << 1
            << user_account_keys::BIRTH_DAY_OF_YEAR << 1
            << user_account_keys::PICTURES << 1
            << user_account_keys::CHAT_ROOMS << 1
            << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<true>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

        if (!runInitialLoginOperation(
                find_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document,
                callback_session)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        ReturnStatus return_status = checkForValidLoginToken(find_user_account, user_account_oid_str);

        if (return_status != ReturnStatus::SUCCESS) {

            //NOTE: Could allow for this to be run while suspended or banned, however because the
            // getLoginDocument() result fizzles if it hits one of these it could cause problems if some
            // other condition in the login check is false. Also, I don't see any benefit of it. If
            // there is a problem with the account they can simply call it when the account is
            // no longer closed
            response->set_return_status(return_status);
        }
        else {

            //setting success here, then it will be changed to error if anything fails
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());

            try {

                //Run some clean up to make sure data is valid
                const bsoncxx::document::view user_account_doc_view = find_user_account->view();

                const int user_age = extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::AGE
                );

                const auto account_status = UserAccountStatus(
                                extractFromBsoncxx_k_int32(
                                        user_account_doc_view,
                                        user_account_keys::STATUS
                                )
                        );

                if (account_status == UserAccountStatus::STATUS_ACTIVE
                    && (user_age < server_parameter_restrictions::LOWEST_ALLOWED_AGE
                        || server_parameter_restrictions::HIGHEST_ALLOWED_AGE < user_age)
                ) {

                    int birth_year = extractFromBsoncxx_k_int32(
                            user_account_doc_view,
                            user_account_keys::BIRTH_YEAR
                    );

                    int birth_month = extractFromBsoncxx_k_int32(
                            user_account_doc_view,
                            user_account_keys::BIRTH_MONTH
                    );

                    int birth_day_of_month = extractFromBsoncxx_k_int32(
                            user_account_doc_view,
                            user_account_keys::BIRTH_DAY_OF_MONTH
                    );

                    int birth_day_of_year = extractFromBsoncxx_k_int32(
                            user_account_doc_view,
                            user_account_keys::BIRTH_DAY_OF_YEAR
                    );

                    const int new_user_age = server_parameter_restrictions::DEFAULT_USER_AGE_IF_ERROR;

                    generateBirthYearForPassedAge(
                            new_user_age,
                            birth_year,
                            birth_month,
                            birth_day_of_month,
                            birth_day_of_year
                    );

                    //Generate the default age range for this users' new age.
                    const AgeRangeDataObject default_age_range = calculateAgeRangeFromUserAge(new_user_age);

                    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
                    std::optional<std::string> update_user_account_exception_string;
                    try {

                        update_user_account = user_accounts_collection.update_one(
                            *callback_session,
                            document{}
                                << "_id" << user_account_oid
                            << finalize,
                            document{}
                                << "$set" << open_document
                                    << user_account_keys::AGE << new_user_age
                                    << user_account_keys::BIRTH_YEAR << birth_year
                                    << user_account_keys::BIRTH_MONTH << birth_month
                                    << user_account_keys::BIRTH_DAY_OF_MONTH << birth_day_of_month
                                    << user_account_keys::BIRTH_DAY_OF_YEAR << birth_day_of_year
                                    << user_account_keys::AGE_RANGE << open_document
                                        << user_account_keys::age_range::MIN << default_age_range.min_age
                                        << user_account_keys::age_range::MAX << default_age_range.max_age
                                    << close_document
                                << close_document
                            << finalize
                        );

                    }
                    catch (const mongocxx::logic_error& e) {
                        update_user_account_exception_string = e.what();
                    }

                    if (!update_user_account) {
                        const std::string error_string = "User account failed to update age after it was already found.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                update_user_account_exception_string, error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "newUserAge", std::to_string(new_user_age),
                                "userAccountOID", user_account_oid,
                                "document", user_account_doc_view
                        );

                        //NOTE: might as well continue here
                    }
                }

                const bsoncxx::array::view account_pictures_array = extractFromBsoncxx_k_array(
                        user_account_doc_view,
                        user_account_keys::PICTURES
                );

                int picture_index = 0;

                //verify pictures
                for (const auto& pic_element : account_pictures_array) {

                    if(!pic_element || pic_element.type() != bsoncxx::type::k_document) {
                        if(!pic_element || pic_element.type() != bsoncxx::type::k_null) {
                            const std::string error_string = "Picture element was incorrect type.";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "user_account_oid", user_account_oid,
                                    "picture_index", std::to_string(picture_index),
                                    "user_account_doc_view", user_account_doc_view,
                                    "pic_element", !pic_element ? "does not exist" : convertBsonTypeToString(pic_element.type())
                            );

                            //NOTE: ok to continue here
                        }

                        if (picture_index >= general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) { //if too many pictures stored
                            const std::string error_string = "Picture array was longer than allowed (null element).";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "user_account_oid", user_account_oid,
                                    "user_account_doc_view", user_account_doc_view,
                                    "picture_index", std::to_string(picture_index)
                            );
                        }

                        picture_index++;
                        continue;
                    }

                    const bsoncxx::document::view pic_document = extractFromBsoncxxArrayElement_k_document(
                            pic_element
                    );

                    const std::chrono::milliseconds account_picture_timestamp = extractFromBsoncxx_k_date(
                            pic_document,
                            user_account_keys::pictures::TIMESTAMP_STORED
                    ).value;

                    const bsoncxx::oid account_picture_oid = extractFromBsoncxx_k_oid(
                            pic_document,
                            user_account_keys::pictures::OID_REFERENCE
                    );

                    if (picture_index >= general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) { //if too many pictures stored

                        const std::string error_string = "Picture array was longer than allowed.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "user_account_oid", user_account_oid,
                                "picture_document_oid", account_picture_oid,
                                "user_account_doc_view", user_account_doc_view,
                                "picture_index", std::to_string(picture_index)
                        );

                        //don't end here even if function fails (logging out, need to try to 'clear' info)
                        findAndDeletePictureDocument(
                                mongo_cpp_client,
                                user_pictures_collection,
                                account_picture_oid,
                                current_timestamp,
                                callback_session
                        );

                        //NOTE: Will trim array elements below.

                    } else { //if index of this picture is within acceptable range

                        bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
                        std::optional<std::string> update_user_account_exception_string;
                        try {

                            //update the picture document
                            update_user_account = user_pictures_collection.update_one(
                                  *callback_session,
                                  document{}
                                      << "_id" << account_picture_oid
                                      << "$expr" << open_document
                                          << "$and" << open_array
                                              << open_document
                                                  << "$eq" << open_array
                                                      << open_document
                                                          << "$strLenBytes" << "$" + user_pictures_keys::THUMBNAIL_IN_BYTES
                                                      << close_document
                                                      << "$" + user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES
                                                  << close_array
                                              << close_document
                                              << open_document
                                                  << "$eq" << open_array
                                                      << open_document
                                                          << "$strLenBytes" << "$" + user_pictures_keys::PICTURE_IN_BYTES
                                                      << close_document
                                                      << "$" + user_pictures_keys::PICTURE_SIZE_IN_BYTES
                                                  << close_array
                                              << close_document
                                          << close_array
                                      << close_document
                                  << finalize,
                              document{}
                                  << "$set" << open_document
                                      << user_pictures_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{account_picture_timestamp}
                                      << user_pictures_keys::PICTURE_INDEX << bsoncxx::types::b_int32{picture_index}
                                  << close_document
                              << finalize
                            );

                        }
                        catch (const mongocxx::logic_error& e) {
                            update_user_account_exception_string = e.what();
                        }

                        if (!update_user_account) {
                            const std::string error_string = "failed to update picture document";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    update_user_account_exception_string, error_string,
                                    "user_account_oid", user_account_oid,
                                    "picture_document_oid", account_picture_oid,
                                    "user_account_doc_view", user_account_doc_view,
                                    "picture_index", std::to_string(picture_index)
                            );

                        } else if (update_user_account->matched_count() == 0) { //update succeeded however picture document was not found OR was corrupt

                            const std::string error_string = "Picture document was not found or corrupt.";
                            //don't end here even if function fails (logging out, need to try to 'clear' info)
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    update_user_account_exception_string, error_string,
                                    "user_account_oid", user_account_oid,
                                    "picture_document_oid", account_picture_oid,
                                    "user_account_doc_view", user_account_doc_view,
                                    "picture_index", std::to_string(picture_index)
                            );

                            removePictureArrayElement(
                                    picture_index,
                                    user_accounts_collection,
                                    user_account_doc_view,
                                    user_account_oid,
                                    callback_session
                            );

                            findAndDeletePictureDocument(
                                    mongo_cpp_client,
                                    user_pictures_collection,
                                        account_picture_oid,
                                        current_timestamp,
                                        callback_session
                            );
                        }
                    }

                    picture_index++;
                }

                if(picture_index != general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {

                    std::optional<std::string> exception_string;
                    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
                    try {

                        if(picture_index > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) { //too many elements stored

                            mongocxx::pipeline pipe;

                            //remove all extra elements
                            pipe.add_fields(
                                document{}
                                    << user_account_keys::PICTURES << open_document
                                        << "$slice" << open_array
                                            << "$" + user_account_keys::PICTURES
                                            << general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT
                                        << close_array
                                    << close_document
                                << finalize
                            );

                            update_user_account = user_accounts_collection.update_one(
                                *callback_session,
                                document{}
                                    << "_id" << user_account_oid
                                << finalize,
                                pipe
                            );

                        } else { //not enough elements stored

                            const std::string error_string = "Picture array was shorter than allowed.";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "user_account_oid", user_account_oid,
                                    "user_account_doc_view", user_account_doc_view,
                                    "picture_index", std::to_string(picture_index)
                            );

                            bsoncxx::builder::basic::array array_to_concatenate;

                            for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - picture_index; ++i) {
                                array_to_concatenate.append(bsoncxx::types::b_null{});
                            }

                            //append any missing elements to the array to keep it consistent with NUMBER_PICTURES_STORED_PER_ACCOUNT
                            update_user_account = user_accounts_collection.update_one(
                                    *callback_session,
                                    document{}
                                        << "_id" << user_account_oid
                                    << finalize,
                                    document{}
                                        << "$push" << open_document
                                            << user_account_keys::PICTURES << open_document
                                                << "$each" << array_to_concatenate
                                            << close_document
                                        << close_document
                                    << finalize
                            );

                        }

                    } catch (const mongocxx::logic_error& e) {
                        exception_string = std::string(e.what());
                    }

                    if (!update_user_account || update_user_account->matched_count() < 1) {
                        const std::string error_string = "Pulling from picture array failed during logout.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                exception_string, error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "user_account_doc_view", user_account_doc_view,
                                "user_account_oid", user_account_oid,
                                "matched_count", std::to_string(update_user_account ? update_user_account->matched_count() : -1)
                        );
                    }
                }

                const bsoncxx::array::view chat_rooms_array = extractFromBsoncxx_k_array(
                        user_account_doc_view,
                        user_account_keys::CHAT_ROOMS
                );

                //possibilities for CHAT_ROOMS
                //1) exists inside OTHER_ACCOUNTS_MATCHED_WITH and not anywhere in any chat rooms;       LEAK; REMOVE FROM OTHER_ACCOUNTS_MATCHED_WITH
                //2) exists inside chat room and not in OTHER_ACCOUNTS_MATCHED_WITH;       NOT SURE; could add it to OTHER_ACCOUNTS_MATCHED_WITH to avoid duplicate matches I guess
                //3) exists inside OTHER_ACCOUNTS_MATCHED_WITH and NOT inside CHAT_ROOM_ID as a matching chat room;       LEAK; REMOVE FROM OTHER_ACCOUNTS_MATCHED_WITH
                //4) exists inside CHAT_ROOM_ID as a matching chat room and NOT inside OTHER_ACCOUNTS_MATCHED_WITH; NOT SURE; could add it to OTHER_ACCOUNTS_MATCHED_WITH to avoid duplicate matches I guess

                //problem want to access chat rooms from OTHER_ACCOUNTS_MATCHED_WITH, but already accessing them from CHAT_ROOM_ID
                //1) get all MATCHING chat rooms from CHAT_ROOM_ID
                //2) line them up with OTHER_ACCOUNTS_MATCHED_WITH
                //3) react according to above

                std::vector<std::string> matching_users_by_chat_room;

                //verify chat rooms
                for (const auto& chat_room : chat_rooms_array) {

                    if(!chat_room || chat_room.type() != bsoncxx::type::k_document) {
                        const std::string error_string = "Chat room element was incorrect type.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "user_account_doc_view", user_account_doc_view,
                                "pic_element", !chat_room ? "does not exist" : convertBsonTypeToString(chat_room.type())
                        );

                        //NOTE: Ok to continue here.

                        continue;
                    }

                    const bsoncxx::document::view chat_room_doc = chat_room.get_document().value;
                    const std::string chat_room_id = extractFromBsoncxx_k_utf8(
                            chat_room_doc,
                            user_account_keys::chat_rooms::CHAT_ROOM_ID
                    );

                    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

                    bsoncxx::stdx::optional<bsoncxx::document::value> chat_room_header_document;
                    std::optional<std::string> chat_room_header_exception_string;
                    try {

                        mongocxx::options::find find_options;
                        find_options.projection(
                            document{}
                                << "_id" << 0
                                << chat_room_header_keys::MATCHING_OID_STRINGS << 1
                            << finalize
                        );

                        chat_room_header_document = chat_room_collection.find_one(
                            *callback_session,
                            document{}
                                << "_id" << chat_room_header_keys::ID
                                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                                    << "$elemMatch" << open_document
                                        << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                        << "$or" << open_array
                                            << open_document
                                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                                            << close_document
                                            << open_document
                                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                                            << close_document
                                        << close_array
                                    << close_document
                                << close_document
                            << finalize,
                            find_options
                        );
                    }
                    catch (const mongocxx::logic_error& e) {
                        chat_room_header_exception_string = e.what();
                    }

                    if (!chat_room_header_document) { //document was not found

                        const std::string error_string = "Chat room was not found or user was not a member of it";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                chat_room_header_exception_string, error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                                "current_object_oid", user_account_oid
                        );

                        bsoncxx::stdx::optional<mongocxx::v_noabi::result::update> update_user_account_chat_rooms;
                        std::optional<std::string> update_user_account_chat_rooms_exception_string;
                        try {

                            //remove verified chat room array element
                            update_user_account_chat_rooms = user_accounts_collection.update_one(
                                *callback_session,
                                document{}
                                    << "_id" << user_account_oid
                                << finalize,
                                     document{}
                                    << "$pull" << open_document
                                        << user_account_keys::CHAT_ROOMS << open_document
                                        << user_account_keys::chat_rooms::CHAT_ROOM_ID << chat_room_id
                                    << close_document
                                    << close_document
                                << finalize
                            );
                        }
                        catch (const mongocxx::logic_error& e) {
                            update_user_account_chat_rooms_exception_string = e.what();
                        }

                        if (!update_user_account_chat_rooms) {

                            const std::string update_veri_chat_rooms_error_string = "Update user account to remove chat room failed\n";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    update_user_account_chat_rooms_exception_string, update_veri_chat_rooms_error_string,
                                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                    "collection", chat_room_collection.name().to_string(),
                                    "ObjectID_used", user_account_oid
                            );

                            //NOTE: ok to continue here
                        }
                    }
                    else { //document was found

                        const bsoncxx::document::view chat_room_header_document_view = chat_room_header_document->view();

                        //extract matching info
                        auto matching_oid_strings_element = chat_room_header_document_view[chat_room_header_keys::MATCHING_OID_STRINGS];
                        if (matching_oid_strings_element
                            && matching_oid_strings_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
                            const bsoncxx::array::view matching_oid_strings = matching_oid_strings_element.get_array().value;

                            for(const auto& element : matching_oid_strings) {
                                if(element.type() == bsoncxx::type::k_utf8) {
                                    std::string matching_account_oid_str = element.get_string().value.to_string();
                                    //if this string is NOT the current user and is the matching user
                                    if(user_account_oid_str != matching_account_oid_str) {
                                        matching_users_by_chat_room.emplace_back(std::move(matching_account_oid_str));
                                        break;
                                    }
                                } else {
                                    logElementError(
                                            __LINE__, __FILE__,
                                            matching_oid_strings_element, chat_room_header_document_view,
                                            bsoncxx::type::k_utf8, chat_room_header_keys::MATCHING_OID_STRINGS,
                                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                                    );
                                    break;
                                }
                            }
                        } else if(!matching_oid_strings_element
                            || matching_oid_strings_element.type() != bsoncxx::type::k_null) { //if element does not exist or is not type utf8 or null
                            logElementError(
                                    __LINE__, __FILE__,
                                    matching_oid_strings_element, chat_room_header_document_view,
                                    bsoncxx::type::k_null, chat_room_header_keys::MATCHING_OID_STRINGS,
                                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                            );

                            break;
                        }
                    }

                }

                const bsoncxx::array::view other_users_matched_with_view = extractFromBsoncxx_k_array(
                        user_account_doc_view,
                        user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH
                );

                std::vector<std::string> other_users_matched_with;

                for(const auto& other_user_element : other_users_matched_with_view) {
                    const bsoncxx::document::view other_user_match_view = extractFromBsoncxxArrayElement_k_document(
                            other_user_element
                    );

                    other_users_matched_with.emplace_back(
                            extractFromBsoncxx_k_utf8(
                                    other_user_match_view,
                                    user_account_keys::other_accounts_matched_with::OID_STRING
                            )
                    );
                }

                std::sort(matching_users_by_chat_room.begin(), matching_users_by_chat_room.end());
                std::sort(other_users_matched_with.begin(), other_users_matched_with.end());

                bsoncxx::builder::basic::array matching_accounts_to_add;
                bsoncxx::builder::basic::array matching_accounts_to_remove;

                //comparing OTHER_ACCOUNTS_MATCHED_WITH with matching chat rooms from CHAT_ROOM_ID
                unsigned int matching_users_chat_room_index = 0;
                unsigned int other_users_matched_with_index = 0;
                while(matching_users_chat_room_index < matching_users_by_chat_room.size() && other_users_matched_with_index < other_users_matched_with.size()) {
                    if(matching_users_by_chat_room[matching_users_chat_room_index] == other_users_matched_with[other_users_matched_with_index]) {
                        //exists inside both, this is OK
                        matching_users_chat_room_index++;
                        other_users_matched_with_index++;
                    }
                    else if(matching_users_by_chat_room[matching_users_chat_room_index] < other_users_matched_with[other_users_matched_with_index]) {

                        //exists as a 'match made' chat room, however NOT inside the list of original_user_account list of matching users

                        runAddMatchingAccount(
                                matching_users_by_chat_room,
                                other_users_matched_with,
                                user_account_oid,
                                current_timestamp,
                                matching_users_chat_room_index,
                                matching_accounts_to_add
                            );

                        matching_users_chat_room_index++;
                    }
                    else { //matching_users_by_chat_room[matching_users_chat_room_index] > other_users_matched_with[other_users_matched_with_index]

                        //exists inside the list of original_user_account list of matching users; however NOT as a 'match made' chat room

                        runRemoveMatchingAccount(
                                matching_users_by_chat_room,
                                other_users_matched_with,
                                user_account_oid,
                                other_users_matched_with_index,
                                matching_accounts_to_remove
                            );

                        other_users_matched_with_index++;
                    }
                }

                while(matching_users_chat_room_index < matching_users_by_chat_room.size()) {
                    //exists as a 'match made' chat room, however NOT inside the list of original_user_account list of matching users

                    runAddMatchingAccount(
                            matching_users_by_chat_room,
                            other_users_matched_with,
                            user_account_oid,
                            current_timestamp,
                            matching_users_chat_room_index,
                            matching_accounts_to_add
                            );

                    matching_users_chat_room_index++;
                }

                while(other_users_matched_with_index < other_users_matched_with.size()) {
                    //exists inside the list of original_user_account list of matching users; however NOT as a 'match made' chat room

                    runRemoveMatchingAccount(
                            matching_users_by_chat_room,
                            other_users_matched_with,
                            user_account_oid,
                            other_users_matched_with_index,
                            matching_accounts_to_remove
                            );

                    other_users_matched_with_index++;
                }

                const bsoncxx::array::view matching_accounts_to_add_view = matching_accounts_to_add.view();
                const bsoncxx::array::view matching_accounts_to_remove_view = matching_accounts_to_remove.view();

                //add or remove any inconsistent data between matching chat rooms from OTHER_ACCOUNTS_MATCHED_WITH
                // and CHAT_ROOM_ID
                if(!matching_accounts_to_add_view.empty() || !matching_accounts_to_remove_view.empty()) {

                    mongocxx::pipeline pipe;

                    pipe.add_fields(
                        document{}
                            << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_document
                                << "$filter" << open_document
                                    << "input" << open_document
                                        << "$concatArrays" << open_array
                                            << "$" + user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH
                                            << matching_accounts_to_add_view
                                        << close_array
                                    << close_document
                                    << "as" << "acc"
                                    << "cond" << open_document
                                        << "$not" << open_array
                                            << open_document
                                                << "$in" << open_array
                                                    << std::string("$$acc.").append(user_account_keys::other_accounts_matched_with::OID_STRING)
                                                    << matching_accounts_to_remove_view
                                                << close_array
                                            << close_document
                                        << close_array
                                    << close_document
                                << close_document
                            << close_document
                        << finalize
                    );

                    try {

                        //remove relevant matches inside MATCHING_OID_STRINGS
                        user_accounts_collection.update_one(
                                *callback_session,
                                document{}
                                    << "_id" << user_account_oid
                                << finalize,
                                pipe
                        );

                    } catch (const mongocxx::logic_error& e) {
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), std::string(e.what()),
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "ObjectID_used", user_account_oid,
                                "matching_accounts_to_add_view", matching_accounts_to_add_view,
                                "matching_accounts_to_remove_view", matching_accounts_to_remove_view
                        );

                        response->set_return_status(ReturnStatus::LG_ERROR);
                        return;
                    }
                }

            } catch (const ErrorExtractingFromBsoncxx& e) {
                //NOTE: Error already stored.
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

        }

    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling logoutFunctionMongoDbImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid_str
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    if(response->return_status() == SUCCESS) {
        auto push_update_doc = document{}
            << user_account_statistics_keys::ACCOUNT_LOGGED_OUT_TIMES << bsoncxx::types::b_date{current_timestamp}
        << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                user_account_oid,
                push_update_doc,
                current_timestamp
                );
    }

}

void runAddMatchingAccount(
        const std::vector<std::string>& matching_users_by_chat_room,
        const std::vector<std::string>& other_users_matched_with,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        const unsigned int matching_users_chat_room_index,
        bsoncxx::builder::basic::array& matching_accounts_to_add
        ) {

    std::string error_string = std::string("User was found to exist inside of a 'match made' chat room "
                                          "from CHAT_ROOM_ID. However NOT inside of OTHER_ACCOUNTS_MATCHED_WITH.\n")
                                                  .append("\nmatching_users_by_chat_room:\n");
    for(const std::string& val : matching_users_by_chat_room) {
        error_string.append(val + '\n');
    }

    error_string.append("\nother_users_matched_with:\n");
    for(const std::string& val : other_users_matched_with) {
        error_string.append(val + '\n');
    }

    storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "user_account_oid", user_account_oid
    );

    matching_accounts_to_add.append(
        document{}
            << user_account_keys::other_accounts_matched_with::OID_STRING << matching_users_by_chat_room[matching_users_chat_room_index]
            << user_account_keys::other_accounts_matched_with::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
        << finalize
    );
}

void runRemoveMatchingAccount(
        const std::vector<std::string>& matching_users_by_chat_room,
        const std::vector<std::string>& other_users_matched_with,
        const bsoncxx::oid& user_account_oid,
        const unsigned int other_users_matched_with_index,
        bsoncxx::builder::basic::array& matching_accounts_to_remove
        ) {

    std::string error_string = std::string("User was found to exist inside of OTHER_ACCOUNTS_MATCHED_WITH. However"
                                           " NOT inside of a 'match made' chat room from CHAT_ROOM_ID.\n")
                                                   .append("\nmatching_users_by_chat_room:\n");
    for(const std::string& val : matching_users_by_chat_room) {
        error_string.append(val + '\n');
    }

    error_string.append("\nother_users_matched_with:\n");
    for(const std::string& val : other_users_matched_with) {
        error_string.append(val + '\n');
    }

    storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "user_account_oid", user_account_oid
    );

    matching_accounts_to_remove.append(other_users_matched_with[other_users_matched_with_index]);
}