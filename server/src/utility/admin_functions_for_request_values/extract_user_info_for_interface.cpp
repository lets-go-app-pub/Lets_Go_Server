//
// Created by jeremiah on 9/23/21.
//

#include "admin_functions_for_request_values.h"

#include <mongocxx/exception/logic_error.hpp>

#include "extract_data_from_bsoncxx.h"
#include "request_helper_functions.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "general_values.h"
#include "empty_or_deleted_picture_info.h"

bool extractUserInfo(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::database& deleted_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& deleted_user_accounts_collection,
        const bool requesting_by_user_account_oid,
        const bsoncxx::document::view& query_doc,
        CompleteUserAccountInfo* user_account_info,
        const std::function<void(const std::string& error_string)>& error_fxn
        ) {

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_document;
    try {
        find_user_document = user_accounts_collection.find_one(
                query_doc
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        error_fxn("Error stored on server.");
        return false;
    }

    if (!find_user_document) {

        //if query doc does NOT search by user_account_oid, do not attempt to search
        // deleted collection
        if(!requesting_by_user_account_oid) {
            error_fxn("Account not found.");
            return false;
        }

        try {
            find_user_document = deleted_user_accounts_collection.find_one(
                    query_doc);
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
            );

            error_fxn("Error stored on server.");
            return false;
        }

        if(!find_user_document) {
            error_fxn("Account not found.");
            return false;
        } else {
            user_account_info->set_account_deleted(true);
        }
    }

    bsoncxx::document::view user_account_doc_view = *find_user_document;

    try {

        bsoncxx::oid user_account_oid = extractFromBsoncxx_k_oid(
                user_account_doc_view,
                "_id");

        user_account_info->set_timestamp_user_returned(current_timestamp.count());

        user_account_info->set_account_oid(user_account_oid.to_string());

        user_account_info->set_account_status(
                UserAccountStatus(
                        extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::STATUS)
                )
        );

        user_account_info->set_inactive_message(
                extractFromBsoncxx_k_utf8(
                        user_account_doc_view,
                        user_account_keys::INACTIVE_MESSAGE)
                        );

        user_account_info->set_number_of_times_timed_out(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_OF_TIMES_TIMED_OUT)
                );

        long inactive_time = -1;

        auto inactive_end_time_element = user_account_doc_view[user_account_keys::INACTIVE_END_TIME];
        if (inactive_end_time_element && inactive_end_time_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
            inactive_time = inactive_end_time_element.get_date().value.count();
        } else if(inactive_end_time_element.type() != bsoncxx::type::k_null) { //if element does not exist or is not type date OR null
            logElementError(
                    __LINE__, __FILE__,
                    inactive_end_time_element, user_account_doc_view,
                    bsoncxx::type::k_null, user_account_keys::INACTIVE_END_TIME,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            throw ErrorExtractingFromBsoncxx("Error requesting user " + user_account_keys::INACTIVE_END_TIME + ".");
        }

        user_account_info->set_inactive_end_time(inactive_time);

        bsoncxx::array::view disciplinary_record = extractFromBsoncxx_k_array(
                user_account_doc_view,
                user_account_keys::DISCIPLINARY_RECORD);

        for (const auto& record : disciplinary_record) {

            bsoncxx::document::view record_doc_view = extractFromBsoncxxArrayElement_k_document(
                    record
            );

            auto disciplinary_action = user_account_info->add_disciplinary_actions();

            disciplinary_action->set_timestamp_submitted(
                    extractFromBsoncxx_k_date(
                            record_doc_view,
                            user_account_keys::disciplinary_record::SUBMITTED_TIME).value.count()
                            );

            disciplinary_action->set_timestamp_ends(
                    extractFromBsoncxx_k_date(
                            record_doc_view,
                            user_account_keys::disciplinary_record::END_TIME).value.count()
                            );

            disciplinary_action->set_type(
                    DisciplinaryActionTypeEnum(
                            extractFromBsoncxx_k_int32(
                                    record_doc_view,
                                    user_account_keys::disciplinary_record::ACTION_TYPE)
                                    )
                                    );

            disciplinary_action->set_reason(
                    extractFromBsoncxx_k_utf8(
                            record_doc_view,
                            user_account_keys::disciplinary_record::REASON)
                            );

            disciplinary_action->set_admin_name(
                    extractFromBsoncxx_k_utf8(
                            record_doc_view,
                            user_account_keys::disciplinary_record::ADMIN_NAME)
                        );
        }

        user_account_info->set_last_time_account_verified(
                extractFromBsoncxx_k_date(
                        user_account_doc_view,
                        user_account_keys::LAST_VERIFIED_TIME).value.count()
                        );

        user_account_info->set_time_account_created(
                extractFromBsoncxx_k_date(
                        user_account_doc_view,
                        user_account_keys::TIME_CREATED).value.count()
                        );

        user_account_info->set_phone_number(
                extractFromBsoncxx_k_utf8(
                        user_account_doc_view,
                        user_account_keys::PHONE_NUMBER)
                        );

        if (!requestNameHelper(
                user_account_doc_view,
                user_account_info->mutable_user_name())) {
            user_account_info->Clear();
            error_fxn("Error requesting user name info.");
            return false;
        }

        if (!requestGenderHelper(
                user_account_doc_view,
                user_account_info->mutable_gender())) {
            user_account_info->Clear();
            error_fxn("Error requesting user gender info.");
            return false;
        }

        if (!requestBirthdayHelper(
                user_account_doc_view,
                user_account_info->mutable_birthday_info())) {
            user_account_info->Clear();
            error_fxn("Error requesting user birthday info.");
            return false;
        }

        if (!requestEmailHelper(
                user_account_doc_view,
                user_account_info->mutable_email_info())) {
            user_account_info->Clear();
            error_fxn("Error requesting user email info.");
            return false;
        }

        user_account_info->set_algorithm_search_options(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::SEARCH_BY_OPTIONS)
        );

        if (!requestCategoriesHelper(
                user_account_doc_view,
                user_account_info->mutable_categories_array())
                ) {
            user_account_info->Clear();
            error_fxn("Error requesting user categories and activities info.");
            return false;
        }

        if (!requestPostLoginInfoHelper(
                user_account_doc_view,
                user_account_info->mutable_post_login_info()
        )) {
            user_account_info->Clear();
            error_fxn("Error requesting user post login info.");
            return false;
        }

        bsoncxx::document::view location_doc = extractFromBsoncxx_k_document(
                user_account_doc_view,
                user_account_keys::LOCATION);

        bsoncxx::array::view coordinates_array = extractFromBsoncxx_k_array(
                location_doc,
                "coordinates");

        auto location_message = user_account_info->mutable_location();
        int coordinates_index = 0;
        for(const auto& val : coordinates_array) {
            if(val.type() == bsoncxx::type::k_double) {
                if(coordinates_index == 0) {
                    location_message->set_client_longitude(val.get_double().value);
                } else if(coordinates_index == 1) {
                    location_message->set_client_latitude(val.get_double().value);
                } else {
                    user_account_info->Clear();
                    error_fxn("Error requesting user location info, 'coordinates' were too long.");
                    return false;
                }
            } else {
                user_account_info->Clear();
                error_fxn("Error requesting user location info, 'coordinates' were incorrect type.");
                return false;
            }

            coordinates_index++;
        }

        user_account_info->set_timestamp_find_matches_last_ran(
                extractFromBsoncxx_k_date(
                        user_account_doc_view,
                        user_account_keys::LAST_TIME_FIND_MATCHES_RAN).value.count()
                        );

        std::function<void(int)> set_picture_empty_response =
                [&user_account_info, &current_timestamp](int indexNumber) {

            auto picture = user_account_info->add_picture_list();

                    EmptyOrDeletedPictureInfo::savePictureInfoDesktopInterface(
                            picture,
                            current_timestamp,
                            indexNumber
                    );
        };

        auto set_picture_to_response =
                [&](
                        const bsoncxx::oid& picture_oid,
                        std::string&& pictureByteString,
                        int pictureSize,
                        int indexNumber,
                        const std::chrono::milliseconds& picture_timestamp,
                        bool extracted_from_deleted_pictures,
                        bool references_removed_after_delete
                        ) {

            auto picture = user_account_info->add_picture_list();

            picture->set_picture_is_deleted(extracted_from_deleted_pictures);
            picture->set_picture_references_have_been_deleted(references_removed_after_delete);

            if(!references_removed_after_delete) {
                picture->set_timestamp(picture_timestamp.count());
                PictureMessage* pictureMessage = picture->mutable_picture();
                pictureMessage->set_file_size(pictureSize);
                pictureMessage->set_file_in_bytes(std::move(pictureByteString));
                pictureMessage->set_index_number(indexNumber);
                pictureMessage->set_picture_oid(picture_oid.to_string());
            } else {
                EmptyOrDeletedPictureInfo::savePictureInfoDesktopInterface(
                        picture,
                        picture_timestamp,
                        indexNumber
                );
            }
        };

        std::set<int> all_picture_indexes;

        for (int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
            all_picture_indexes.insert(i);
        }

        if (!requestPicturesHelper(
                all_picture_indexes,
                accounts_db,
                user_account_oid,
                user_account_doc_view,
                mongo_cpp_client,
                user_accounts_collection,
                current_timestamp,
                set_picture_empty_response,
                set_picture_to_response,
                nullptr,
                &deleted_db)
        ) {
            user_account_info->Clear();
            error_fxn("Error requesting user pictures.");
            return false;
        }

        std::sort(user_account_info->mutable_picture_list()->begin(), user_account_info->mutable_picture_list()->end(), [](
                const PictureMessageResult& lhs, const PictureMessageResult& rhs
                ){
            return lhs.picture().index_number() < rhs.picture().index_number();
        });

        bsoncxx::array::view other_users_blocked = extractFromBsoncxx_k_array(
                user_account_doc_view,
                user_account_keys::OTHER_USERS_BLOCKED);

        user_account_info->set_number_of_other_users_blocked(
                (int) std::distance(other_users_blocked.begin(), other_users_blocked.end()));

        int blocked = 0;
        int reported = 0;

        user_account_info->set_number_of_times_swiped_yes(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SWIPED_YES)
                        );

        user_account_info->set_number_of_times_swiped_no(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SWIPED_NO)
                        );

        blocked = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_SWIPED_BLOCK);

        reported = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_SWIPED_REPORT);

        user_account_info->set_number_of_times_swiped_block_report(reported + blocked);

        user_account_info->set_number_of_times_others_swiped_yes_on(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES)
                        );

        user_account_info->set_number_of_times_others_swiped_no_on(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO)
                        );

        blocked = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK);

        reported = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT);

        user_account_info->set_number_of_times_others_swiped_block_report(reported + blocked);

        user_account_info->set_number_of_times_sent_activity_suggestion(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION)
                        );

        user_account_info->set_number_of_times_sent_bug_report(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT)
                        );

        user_account_info->set_number_of_times_sent_other_suggestion(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION)
                        );

        user_account_info->set_number_of_times_spam_feedback_sent(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT)
                        );

        user_account_info->set_number_of_times_spam_reports_sent(
                extractFromBsoncxx_k_int32(
                        user_account_doc_view,
                        user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT)
                        );

        blocked = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM);

        reported = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM);

        user_account_info->set_number_of_times_spam_reports_sent(reported + blocked);

        blocked = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM);

        reported = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM);

        user_account_info->set_number_of_times_this_user_blocked_reported_from_chat_room(reported + blocked);

        user_account_info->set_opted_in_to_promotional_email(
                extractFromBsoncxx_k_bool(
                        user_account_doc_view,
                        user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL
                )
        );

        user_account_info->set_subscription_status(
                UserSubscriptionStatus(
                        extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::SUBSCRIPTION_STATUS
                        )
                )
        );

        user_account_info->set_subscription_expiration_time_ms(
                extractFromBsoncxx_k_date(
                        user_account_doc_view,
                        user_account_keys::SUBSCRIPTION_EXPIRATION_TIME
                ).value.count()
        );

        user_account_info->set_account_type(
                UserAccountType(
                        extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::ACCOUNT_TYPE
                        )
                )
        );

        const std::chrono::milliseconds event_expiration_time = extractFromBsoncxx_k_date(
                user_account_doc_view,
                user_account_keys::EVENT_EXPIRATION_TIME
        ).value;

        //EVENT_VALUES may not exist
        auto event_values_element = user_account_doc_view[user_account_keys::EVENT_VALUES];
        if (event_values_element && event_values_element.type() == bsoncxx::type::k_document) { //if element exists and is type array
            const bsoncxx::document::view event_value_doc = event_values_element.get_document().value;

            user_account_info->mutable_event_values()->set_created_by(
                    extractFromBsoncxx_k_utf8(
                            event_value_doc,
                            user_account_keys::event_values::CREATED_BY
                    )
            );

            user_account_info->mutable_event_values()->set_chat_room_id(
                    extractFromBsoncxx_k_utf8(
                            event_value_doc,
                            user_account_keys::event_values::CHAT_ROOM_ID
                    )
            );

            user_account_info->mutable_event_values()->set_event_title(
                    extractFromBsoncxx_k_utf8(
                            event_value_doc,
                            user_account_keys::event_values::EVENT_TITLE
                    )
            );

            const LetsGoEventStatus event_status = convertExpirationTimeToEventStatus(
                    event_expiration_time,
                    current_timestamp
            );

            user_account_info->mutable_event_values()->set_event_state(event_status);
        }
        else if(event_values_element) { //if element does not exist or is not type array
            logElementError(
                __LINE__, __FILE__,
                event_values_element, user_account_doc_view,
                bsoncxx::type::k_array, user_account_keys::EVENT_VALUES,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            throw ErrorExtractingFromBsoncxx("Error requesting user info value " + user_account_keys::EVENT_VALUES + ".");
        }

        user_account_info->set_event_expiration_time_ms(event_expiration_time.count());

        const bsoncxx::array::view user_created_events_arr = extractFromBsoncxx_k_array(
                user_account_doc_view,
                user_account_keys::USER_CREATED_EVENTS
        );

        for(const auto& user_created_events_ele : user_created_events_arr) {
            const bsoncxx::document::view user_created_events_doc = extractFromBsoncxxArrayElement_k_document(user_created_events_ele);

            UserCreatedEvent* user_created_events = user_account_info->add_user_created_events();

            user_created_events->set_event_oid(
                    extractFromBsoncxx_k_oid(
                            user_created_events_doc,
                            user_account_keys::user_created_events::EVENT_OID
                    ).to_string()
            );

            user_created_events->set_expiration_time_long(
                    extractFromBsoncxx_k_date(
                            user_created_events_doc,
                            user_account_keys::user_created_events::EXPIRATION_TIME
                    )
            );

            user_created_events->set_event_state(
                    LetsGoEventStatus(
                            extractFromBsoncxx_k_int32(
                                    user_created_events_doc,
                                    user_account_keys::user_created_events::EVENT_STATE
                            )
                    )
            );
        }

        user_account_info->set_last_time_displayed_info_updated(
            extractFromBsoncxx_k_date(
                    user_account_doc_view,
                    user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED
            ).value.count()
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        user_account_info->Clear();
        error_fxn(e.what());
        return false;
    }

    return true;
}