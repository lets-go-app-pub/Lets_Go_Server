#include <user_account_keys.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <connection_pool_global_variable.h>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <utility_testing_functions.h>
#include <utility_general_functions.h>
#include <gtest/gtest.h>
#include <mongocxx/exception/logic_error.hpp>
#include "account_objects.h"
#include "utility_find_matches_functions.h"
#include "deleted_objects.h"
#include "empty_or_deleted_picture_info.h"
#include "EventRequestMessage.pb.h"
#include "activities_info_keys.h"
#include "global_bsoncxx_docs.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


void extractArrayOfMatchingElementsToDocument(
        const std::string& key,
        const std::vector<MatchingElement>& matching_array,
        bsoncxx::builder::stream::document& document_result
) {

    bsoncxx::builder::basic::array matching_array_doc;

    for (const MatchingElement& match_element : matching_array) {

        bsoncxx::builder::stream::document match_element_doc;
        match_element.convertToDocument(match_element_doc);

        matching_array_doc.append(match_element_doc);
    }

    document_result
            << key << matching_array_doc;
}

void UserAccountDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << user_account_keys::STATUS << status
            << user_account_keys::INACTIVE_MESSAGE << inactive_message;

    if (inactive_end_time == bsoncxx::types::b_date{std::chrono::milliseconds{-1L}}) {
        document_result
                << user_account_keys::INACTIVE_END_TIME << bsoncxx::types::b_null{};
    } else {
        document_result
                << user_account_keys::INACTIVE_END_TIME << inactive_end_time;
    }

    document_result
            << user_account_keys::NUMBER_OF_TIMES_TIMED_OUT << number_of_times_timed_out;

    bsoncxx::builder::basic::array installation_ids_doc;

    for (const std::string& installation_id : installation_ids) {
        installation_ids_doc.append(installation_id);
    }

    document_result
            << user_account_keys::INSTALLATION_IDS << installation_ids_doc
            << user_account_keys::LAST_VERIFIED_TIME << last_verified_time
            << user_account_keys::TIME_CREATED << time_created;

    bsoncxx::builder::basic::array account_id_list_doc;

    for (const std::string& account_id : account_id_list) {
        account_id_list_doc.append(account_id);
    }

    document_result
            << user_account_keys::ACCOUNT_ID_LIST << account_id_list_doc
            << user_account_keys::SUBSCRIPTION_STATUS << subscription_status
            << user_account_keys::SUBSCRIPTION_EXPIRATION_TIME << subscription_expiration_time
            << user_account_keys::ACCOUNT_TYPE << account_type
            << user_account_keys::PHONE_NUMBER << phone_number
            << user_account_keys::FIRST_NAME << first_name
            << user_account_keys::BIO << bio
            << user_account_keys::CITY << city
            << user_account_keys::GENDER << gender
            << user_account_keys::BIRTH_YEAR << birth_year
            << user_account_keys::BIRTH_MONTH << birth_month
            << user_account_keys::BIRTH_DAY_OF_MONTH << birth_day_of_month
            << user_account_keys::BIRTH_DAY_OF_YEAR << birth_day_of_year
            << user_account_keys::AGE << age
            << user_account_keys::EMAIL_ADDRESS << email_address
            << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << email_address_requires_verification
            << user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL << opted_in_to_promotional_email;

    bsoncxx::builder::basic::array pictures_doc;

    for (const PictureReference& picture : pictures) {
        if (picture.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1L}};
            picture.getPictureReference(picture_reference, timestamp_stored);

            pictures_doc.append(document{}
                                        << user_account_keys::pictures::OID_REFERENCE << picture_reference
                                        << user_account_keys::pictures::TIMESTAMP_STORED << timestamp_stored
                                        << finalize);
        } else {
            pictures_doc.append(bsoncxx::types::b_null{});
        }
    }

    document_result
            << user_account_keys::PICTURES << pictures_doc
            << user_account_keys::SEARCH_BY_OPTIONS << search_by_options;

    bsoncxx::builder::basic::array categories_doc;

    for (const TestCategory& category : categories) {

        bsoncxx::builder::basic::array timeframes_doc;

        for (const TestTimeframe& time_frame : category.time_frames) {
            timeframes_doc.append(
                    document{}
                            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                            time_frame.time}
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << time_frame.startStopValue
                            << finalize
            );
        }

        categories_doc.append(
                document{}
                        << user_account_keys::categories::TYPE << category.type
                        << user_account_keys::categories::INDEX_VALUE << category.index_value
                        << user_account_keys::categories::TIMEFRAMES << timeframes_doc
                        << finalize
        );
    }

    document_result
            << user_account_keys::CATEGORIES << categories_doc;

    if(event_values) {
        document_result
                << user_account_keys::EVENT_VALUES << open_document
                    << user_account_keys::event_values::CREATED_BY << event_values->created_by
                    << user_account_keys::event_values::CHAT_ROOM_ID << event_values->chat_room_id
                    << user_account_keys::event_values::EVENT_TITLE << event_values->event_title
                << close_document;
    }

    document_result
            << user_account_keys::EVENT_EXPIRATION_TIME << event_expiration_time;

    bsoncxx::builder::basic::array user_created_events_doc;

    for (const UserCreatedEvent& user_created_event : user_created_events) {
        user_created_events_doc.append(
                document{}
                        << user_account_keys::user_created_events::EVENT_OID << user_created_event.event_oid
                        << user_account_keys::user_created_events::EXPIRATION_TIME << user_created_event.expiration_time
                        << user_account_keys::user_created_events::EVENT_STATE << user_created_event.event_state
                << finalize
        );
    }

    document_result
            << user_account_keys::USER_CREATED_EVENTS << user_created_events_doc;

    bsoncxx::builder::basic::array other_accounts_matched_with_doc;

    for (const OtherAccountMatchedWith& other_matched_user : other_accounts_matched_with) {
        other_accounts_matched_with_doc.append(
                document{}
                        << user_account_keys::other_accounts_matched_with::OID_STRING << other_matched_user.oid_string
                        << user_account_keys::other_accounts_matched_with::TIMESTAMP << other_matched_user.timestamp
                        << finalize
        );
    }

    document_result
            << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << other_accounts_matched_with_doc
            << user_account_keys::MATCHING_ACTIVATED << matching_activated
            << user_account_keys::LOCATION << open_document
                << "type" << "Point"
                << "coordinates" << open_array
                    << bsoncxx::v_noabi::types::b_double{location.longitude}
                    << bsoncxx::v_noabi::types::b_double{location.latitude}
                << close_array //some random default coordinates; I think It's somewhere in California?
            << close_document
            << user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK << find_matches_timestamp_lock
            << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << last_time_find_matches_ran
            << user_account_keys::AGE_RANGE << open_document
            << user_account_keys::age_range::MIN << bsoncxx::types::b_int32{age_range.min}
            << user_account_keys::age_range::MAX << bsoncxx::types::b_int32{age_range.max}
            << close_document;

    bsoncxx::builder::basic::array genders_range_doc;

    for (const std::string& individual_gender : genders_range) {
        genders_range_doc.append(individual_gender);
    }

    document_result
            << user_account_keys::GENDERS_RANGE << genders_range_doc
            << user_account_keys::MAX_DISTANCE << max_distance;

    bsoncxx::builder::basic::array previously_matched_accounts_doc;

    for (const PreviouslyMatchedAccounts& previously_matched_account : previously_matched_accounts) {
        previously_matched_accounts_doc.append(
                document{}
                        << user_account_keys::previously_matched_accounts::OID << previously_matched_account.oid
                        << user_account_keys::previously_matched_accounts::TIMESTAMP << previously_matched_account.timestamp
                        << user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED << previously_matched_account.number_times_matched
                        << finalize
        );
    }

    document_result
            << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << previously_matched_accounts_doc
            << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << last_time_empty_match_returned
            << user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN << last_time_match_algorithm_ran
            << user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM << int_for_match_list_to_draw_from
            << user_account_keys::TOTAL_NUMBER_MATCHES_DRAWN << total_number_matches_drawn;

    extractArrayOfMatchingElementsToDocument(
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            has_been_extracted_accounts_list,
            document_result
    );

    extractArrayOfMatchingElementsToDocument(
            user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
            algorithm_matched_accounts_list,
            document_result
    );

    extractArrayOfMatchingElementsToDocument(
            user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
            other_users_matched_accounts_list,
            document_result
    );

    document_result
            << user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN << time_sms_can_be_sent_again
            << user_account_keys::NUMBER_SWIPES_REMAINING << number_swipes_remaining
            << user_account_keys::SWIPES_LAST_UPDATED_TIME << swipes_last_updated_time
            << user_account_keys::LOGGED_IN_TOKEN << logged_in_token
            << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << logged_in_token_expiration
            << user_account_keys::LOGGED_IN_INSTALLATION_ID << logged_in_installation_id
            << user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE << cool_down_on_sms_return_message
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << logged_in_return_message
            << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << time_email_can_be_sent_again
            << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << last_time_displayed_info_updated
            << user_account_keys::BIRTHDAY_TIMESTAMP << birthday_timestamp
            << user_account_keys::GENDER_TIMESTAMP << gender_timestamp
            << user_account_keys::FIRST_NAME_TIMESTAMP << first_name_timestamp
            << user_account_keys::BIO_TIMESTAMP << bio_timestamp
            << user_account_keys::CITY_NAME_TIMESTAMP << city_name_timestamp
            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << post_login_info_timestamp
            << user_account_keys::EMAIL_TIMESTAMP << email_timestamp
            << user_account_keys::CATEGORIES_TIMESTAMP << categories_timestamp;

    bsoncxx::builder::basic::array chat_rooms_doc;

    for (const SingleChatRoom& chat_room : chat_rooms) {
        document chat_room_doc;
        chat_room_doc
                << user_account_keys::chat_rooms::CHAT_ROOM_ID << chat_room.chat_room_id
                << user_account_keys::chat_rooms::LAST_TIME_VIEWED << chat_room.last_time_viewed;

        if(chat_room.event_oid) {
            chat_room_doc
                    << user_account_keys::chat_rooms::EVENT_OID << *chat_room.event_oid;
        }
        chat_rooms_doc.append(chat_room_doc.view());
    }

    document_result
            << user_account_keys::CHAT_ROOMS << chat_rooms_doc;

    bsoncxx::builder::basic::array other_users_blocked_doc;

    for (const OtherBlockedUser& other_blocked_user : other_users_blocked) {
        other_users_blocked_doc.append(
                document{}
                        << user_account_keys::other_users_blocked::OID_STRING << other_blocked_user.oid_string
                        << user_account_keys::other_users_blocked::TIMESTAMP_BLOCKED << other_blocked_user.timestamp_blocked
                        << finalize
        );
    }

    document_result
            << user_account_keys::OTHER_USERS_BLOCKED << other_users_blocked_doc
            << user_account_keys::NUMBER_TIMES_SWIPED_YES << number_times_swiped_yes
            << user_account_keys::NUMBER_TIMES_SWIPED_NO << number_times_swiped_no
            << user_account_keys::NUMBER_TIMES_SWIPED_BLOCK << number_times_swiped_block
            << user_account_keys::NUMBER_TIMES_SWIPED_REPORT << number_times_swiped_report
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << number_times_others_swiped_yes
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO << number_times_others_swiped_no
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK << number_times_others_swiped_block
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT << number_times_others_swiped_report
            << user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION << number_times_sent_activity_suggestion
            << user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT << number_times_sent_bug_report
            << user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION << number_times_sent_other_suggestion
            << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << number_times_spam_feedback_sent
            << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << number_times_spam_reports_sent
            << user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM << number_times_reported_by_others_in_chat_room
            << user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM << number_times_blocked_by_others_in_chat_room
            << user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM << number_times_this_user_reported_from_chat_room
            << user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM << number_times_this_user_blocked_from_chat_room;

    bsoncxx::builder::basic::array disciplinary_record_doc;

    for (const DisciplinaryRecord& single_disciplinary_record : disciplinary_record) {
        disciplinary_record_doc.append(
                document{}
                        << user_account_keys::disciplinary_record::SUBMITTED_TIME << single_disciplinary_record.submitted_time
                        << user_account_keys::disciplinary_record::END_TIME << single_disciplinary_record.end_time
                        << user_account_keys::disciplinary_record::ACTION_TYPE << single_disciplinary_record.action_type
                        << user_account_keys::disciplinary_record::REASON << single_disciplinary_record.reason
                        << user_account_keys::disciplinary_record::ADMIN_NAME << single_disciplinary_record.admin_name
                        << finalize
        );
    }

    document_result
            << user_account_keys::DISCIPLINARY_RECORD << disciplinary_record_doc;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

CompleteUserAccountInfo UserAccountDoc::convertToCompleteUserAccountInfo(const std::chrono::milliseconds& current_timestamp) const {

    CompleteUserAccountInfo user_account_info_msg;

    user_account_info_msg.set_timestamp_user_returned(current_timestamp.count());

    user_account_info_msg.set_account_deleted(false);
    user_account_info_msg.set_account_status(status);
    user_account_info_msg.set_inactive_message(inactive_message);
    user_account_info_msg.set_number_of_times_timed_out(number_of_times_timed_out);
    user_account_info_msg.set_inactive_end_time(inactive_end_time.value.count());

    for(const auto& disc : disciplinary_record) {
        auto* disc_action = user_account_info_msg.add_disciplinary_actions();

        disc_action->set_timestamp_submitted(disc.submitted_time.value.count());
        disc_action->set_timestamp_ends(disc.end_time.value.count());
        disc_action->set_type(disc.action_type);
        disc_action->set_reason(disc.reason);
        disc_action->set_admin_name(disc.admin_name);
    }

    user_account_info_msg.set_last_time_account_verified(last_verified_time);
    user_account_info_msg.set_time_account_created(time_created);
    user_account_info_msg.set_account_oid(current_object_oid.to_string());
    user_account_info_msg.set_phone_number(phone_number);
    user_account_info_msg.set_user_name(first_name);
    user_account_info_msg.set_gender(gender);

    user_account_info_msg.mutable_birthday_info()->set_birth_year(birth_year);
    user_account_info_msg.mutable_birthday_info()->set_birth_month(birth_month);
    user_account_info_msg.mutable_birthday_info()->set_birth_day_of_month(birth_day_of_month);
    user_account_info_msg.mutable_birthday_info()->set_age(age);

    user_account_info_msg.mutable_email_info()->set_email(email_address);
    user_account_info_msg.mutable_email_info()->set_requires_email_verification(email_address_requires_verification);

    user_account_info_msg.set_algorithm_search_options(search_by_options);

//    message CategoryTimeFrameMessage {
//            int64 start_time_frame = 1; //start time is expected to be less than or equal to stop time
//            int64 stop_time_frame = 2; //stop time is expected to be greater than or equal to start time
//    }
//
//    message CategoryActivityMessage {
//            int32 activity_index = 1; //this is the index value of the activity
//            repeated CategoryTimeFrameMessage time_frame_array = 2; //these are the time frames active on the activity
//    };

    for(const auto& category : categories) {
        if(category.type == AccountCategoryType::CATEGORY_TYPE) {
            continue;
        }

        auto* category_ele = user_account_info_msg.add_categories_array();
        category.convertToCategoryActivityMessage(category_ele);
    }

    user_account_info_msg.mutable_post_login_info()->set_user_bio(bio);
    user_account_info_msg.mutable_post_login_info()->set_user_city(city);

    for(const auto& gen : genders_range) {
        user_account_info_msg.mutable_post_login_info()->add_gender_range(gen);
    }

    user_account_info_msg.mutable_post_login_info()->set_min_age(age_range.min);
    user_account_info_msg.mutable_post_login_info()->set_max_age(age_range.max);
    user_account_info_msg.mutable_post_login_info()->set_max_distance(max_distance);

    user_account_info_msg.mutable_location()->set_client_longitude(location.longitude);
    user_account_info_msg.mutable_location()->set_client_latitude(location.latitude);

    user_account_info_msg.set_timestamp_find_matches_last_ran(last_time_find_matches_ran);

    int picture_index_num = 0;
    for(const auto& pic : pictures) {
        if(pic.pictureStored()) {

            bsoncxx::oid pic_reference;
            bsoncxx::types::b_date pic_timestamp{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    pic_reference,
                    pic_timestamp
            );

            UserPictureDoc user_picture(pic_reference);

            if(user_picture.current_object_oid.to_string() == "000000000000000000000000") { //pic not found

                DeletedUserPictureDoc deleted_picture(pic_reference);

                if(deleted_picture.current_object_oid.to_string() == "000000000000000000000000") { //deleted pic not found
                    auto* picture = user_account_info_msg.add_picture_list();

                    picture->set_timestamp(pic_timestamp.value.count());
                    picture->set_picture_is_deleted(true);
                    picture->set_picture_references_have_been_deleted(true);

                    picture->mutable_picture()->set_file_size(user_picture.picture_size_in_bytes);
                    picture->mutable_picture()->set_file_in_bytes(user_picture.picture_in_bytes);
                    picture->mutable_picture()->set_index_number(picture_index_num);
                    picture->mutable_picture()->set_picture_oid(pic_reference.to_string());
                } else {
                    auto* picture = user_account_info_msg.add_picture_list();

                    const bool references_removed_after_delete = deleted_picture.references_removed_after_delete ?
                            *deleted_picture.references_removed_after_delete
                            : false;

                    picture->set_picture_is_deleted(true);
                    picture->set_picture_references_have_been_deleted(references_removed_after_delete);

                    EmptyOrDeletedPictureInfo::savePictureInfoDesktopInterface(
                            picture,
                            pic_timestamp.value,
                            picture_index_num
                    );
                }
            } else {
                auto* picture = user_account_info_msg.add_picture_list();

                picture->set_timestamp(pic_timestamp.value.count());
                picture->set_picture_is_deleted(false);
                picture->set_picture_references_have_been_deleted(false);

                picture->mutable_picture()->set_file_size(user_picture.picture_size_in_bytes);
                picture->mutable_picture()->set_file_in_bytes(user_picture.picture_in_bytes);
                picture->mutable_picture()->set_index_number(picture_index_num);
                picture->mutable_picture()->set_picture_oid(pic_reference.to_string());
            }


        } else {
            auto* picture = user_account_info_msg.add_picture_list();

            EmptyOrDeletedPictureInfo::savePictureInfoDesktopInterface(
                    picture,
                    current_timestamp,
                    picture_index_num
            );
        }
        picture_index_num++;
    }

    user_account_info_msg.set_number_of_other_users_blocked((int)other_users_blocked.size());
    user_account_info_msg.set_number_of_times_swiped_yes(number_times_swiped_yes);
    user_account_info_msg.set_number_of_times_swiped_no(number_times_swiped_no);
    user_account_info_msg.set_number_of_times_swiped_block_report(number_times_swiped_report + number_times_swiped_block);
    user_account_info_msg.set_number_of_times_others_swiped_yes_on(number_times_others_swiped_yes);
    user_account_info_msg.set_number_of_times_others_swiped_no_on(number_times_others_swiped_no);
    user_account_info_msg.set_number_of_times_others_swiped_block_report(number_times_others_swiped_report + number_times_others_swiped_block);
    user_account_info_msg.set_number_of_times_sent_activity_suggestion(number_times_sent_activity_suggestion);
    user_account_info_msg.set_number_of_times_sent_bug_report(number_times_sent_bug_report);
    user_account_info_msg.set_number_of_times_sent_other_suggestion(number_times_sent_other_suggestion);
    user_account_info_msg.set_number_of_times_spam_feedback_sent(number_times_spam_feedback_sent);
    user_account_info_msg.set_number_of_times_spam_reports_sent(number_times_spam_reports_sent);
    user_account_info_msg.set_number_of_times_blocked_reported_by_others_in_chat_room(number_times_reported_by_others_in_chat_room + number_times_blocked_by_others_in_chat_room);
    user_account_info_msg.set_number_of_times_this_user_blocked_reported_from_chat_room(number_times_this_user_reported_from_chat_room + number_times_this_user_blocked_from_chat_room);

    user_account_info_msg.set_last_time_displayed_info_updated(last_time_displayed_info_updated);

    user_account_info_msg.set_opted_in_to_promotional_email(opted_in_to_promotional_email);

    user_account_info_msg.set_subscription_status(subscription_status);
    user_account_info_msg.set_subscription_expiration_time_ms(subscription_expiration_time);
    user_account_info_msg.set_account_type(account_type);

    if(event_values) {
        user_account_info_msg.mutable_event_values()->set_created_by(event_values->created_by);
        user_account_info_msg.mutable_event_values()->set_chat_room_id(event_values->chat_room_id);
        user_account_info_msg.mutable_event_values()->set_event_title(event_values->event_title);

        const LetsGoEventStatus event_status = convertExpirationTimeToEventStatus(
                event_expiration_time.value,
                current_timestamp
        );
        user_account_info_msg.mutable_event_values()->set_event_state(event_status);
    }

    user_account_info_msg.set_event_expiration_time_ms(event_expiration_time.value.count());

    for(const auto& user_created_event : user_created_events) {
        auto* user_created_event_msg = user_account_info_msg.add_user_created_events();

        user_created_event_msg->set_event_oid(
                user_created_event.event_oid.to_string()
        );

        user_created_event_msg->set_expiration_time_long(
                user_created_event.expiration_time
        );

        user_created_event_msg->set_event_state(
                user_created_event.event_state
        );
    }

    return user_account_info_msg;
}

void UserAccountDoc::generateNewUserAccount(
        const bsoncxx::oid& account_oid,
        const std::string& passed_phone_number,
        const std::vector<std::string>& passed_installation_ids,
        const std::vector<std::string>& passed_account_id_list,
        const std::chrono::milliseconds& current_timestamp,
        const std::chrono::milliseconds& passed_time_sms_can_be_sent_again,
        const int passed_number_swipes_remaining,
        const long passed_swipes_last_updated_time,
        const std::chrono::milliseconds& passed_time_email_can_be_sent_again
        ) {
    current_object_oid = account_oid;

    const bsoncxx::types::b_date current_timestamp_date{current_timestamp};

    status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO;
    inactive_message = DEFAULT_STRING;

    inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    number_of_times_timed_out = INT_ZERO;

    installation_ids = passed_installation_ids;

    last_verified_time = current_timestamp_date;
    time_created = current_timestamp_date;

    account_id_list = passed_account_id_list;

    subscription_status = UserSubscriptionStatus::NO_SUBSCRIPTION;
    subscription_expiration_time = DEFAULT_DATE;
    account_type = UserAccountType::USER_ACCOUNT_TYPE;

    phone_number = passed_phone_number;
    first_name = DEFAULT_STRING;
    bio = DEFAULT_STRING;
    city = DEFAULT_STRING;
    gender = DEFAULT_STRING;
    birth_year = DEFAULT_INT;
    birth_month = DEFAULT_INT;
    birth_day_of_month = DEFAULT_INT;
    birth_day_of_year = DEFAULT_INT;
    age = DEFAULT_INT;
    email_address = DEFAULT_STRING;

    email_address_requires_verification = true;
    opted_in_to_promotional_email = true;

    pictures.clear();
    for (int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; ++i) {
        pictures.emplace_back(PictureReference());
    }

    search_by_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    categories = std::vector<TestCategory>{
            TestCategory(AccountCategoryType::ACTIVITY_TYPE, 0),
            TestCategory(AccountCategoryType::CATEGORY_TYPE, 0)
    };

    event_values.reset();

    event_expiration_time = general_values::event_expiration_time_values::USER_ACCOUNT;

    user_created_events.clear();

    other_accounts_matched_with.clear();

    matching_activated = false;

    location = Location(
            -122,
            37
    );

    find_matches_timestamp_lock = DEFAULT_DATE;
    last_time_find_matches_ran = current_timestamp_date;

    age_range = AgeRange(
            -1,
            -1
    );

    genders_range.clear();

    max_distance = server_parameter_restrictions::DEFAULT_MAX_DISTANCE;

    previously_matched_accounts.clear();

    last_time_empty_match_returned = DEFAULT_DATE;
    last_time_match_algorithm_ran = DEFAULT_DATE;

    int_for_match_list_to_draw_from = INT_ZERO;
    total_number_matches_drawn = INT_ZERO;

    has_been_extracted_accounts_list.clear();
    algorithm_matched_accounts_list.clear();
    other_users_matched_accounts_list.clear();

    time_sms_can_be_sent_again = bsoncxx::types::b_date{passed_time_sms_can_be_sent_again};
    number_swipes_remaining = passed_number_swipes_remaining;
    swipes_last_updated_time = passed_swipes_last_updated_time;

    logged_in_token = DEFAULT_STRING;
    logged_in_token_expiration = DEFAULT_DATE;
    logged_in_installation_id = DEFAULT_STRING;

    cool_down_on_sms_return_message = DEFAULT_INT;
    logged_in_return_message = DEFAULT_INT;

    time_email_can_be_sent_again = bsoncxx::types::b_date{passed_time_email_can_be_sent_again};

    last_time_displayed_info_updated = DEFAULT_DATE;

    birthday_timestamp = DEFAULT_DATE;
    gender_timestamp = DEFAULT_DATE;
    first_name_timestamp = DEFAULT_DATE;
    bio_timestamp = DEFAULT_DATE;
    city_name_timestamp = DEFAULT_DATE;
    post_login_info_timestamp = DEFAULT_DATE;
    email_timestamp = DEFAULT_DATE;
    categories_timestamp = DEFAULT_DATE;

    chat_rooms.clear();
    other_users_blocked.clear();

    number_times_swiped_yes = INT_ZERO;
    number_times_swiped_no = INT_ZERO;
    number_times_swiped_block = INT_ZERO;
    number_times_swiped_report = INT_ZERO;

    number_times_others_swiped_yes = INT_ZERO;
    number_times_others_swiped_no = INT_ZERO;
    number_times_others_swiped_block = INT_ZERO;
    number_times_others_swiped_report = INT_ZERO;

    number_times_sent_activity_suggestion = INT_ZERO;
    number_times_sent_bug_report = INT_ZERO;
    number_times_sent_other_suggestion = INT_ZERO;

    number_times_spam_feedback_sent = INT_ZERO;
    number_times_spam_reports_sent = INT_ZERO;

    number_times_reported_by_others_in_chat_room = INT_ZERO;
    number_times_blocked_by_others_in_chat_room = INT_ZERO;
    number_times_this_user_reported_from_chat_room = INT_ZERO;
    number_times_this_user_blocked_from_chat_room = INT_ZERO;

    disciplinary_record.clear();
}

bool UserAccountDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database database = mongo_cpp_client[database_name];
    mongocxx::collection collection = database[collection_name];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool UserAccountDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database database = mongo_cpp_client[database_name];
    mongocxx::collection collection = database[collection_name];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = collection.find_one(
            document{}
                << "_id" << findOID
            << finalize
        );
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

MatchingElement extractMatchingElementToClass(
        const bsoncxx::document::view& match_element_doc
        ) {

    bsoncxx::oid saved_statistics_oid{"000000000000000000000000"};
    auto saved_statistics_oid_element = match_element_doc[user_account_keys::accounts_list::SAVED_STATISTICS_OID];
    if (saved_statistics_oid_element) {
        saved_statistics_oid = saved_statistics_oid_element.get_oid().value;
    }

    return {
        match_element_doc[user_account_keys::accounts_list::OID].get_oid().value,
        match_element_doc[user_account_keys::accounts_list::POINT_VALUE].get_double().value,
        match_element_doc[user_account_keys::accounts_list::DISTANCE].get_double().value,
        match_element_doc[user_account_keys::accounts_list::EXPIRATION_TIME].get_date(),
        match_element_doc[user_account_keys::accounts_list::MATCH_TIMESTAMP].get_date(),
        match_element_doc[user_account_keys::accounts_list::FROM_MATCH_ALGORITHM_LIST].get_bool().value,
        saved_statistics_oid
    };
}

void UserAccountDoc::getFromEventRequestMessageAfterStored(
        const EventRequestMessage& event_request,
        const bsoncxx::oid& event_oid,
        UserAccountType user_account_type,
        const std::chrono::milliseconds& current_timestamp,
        const std::string& event_created_by,
        const std::string& event_chat_room_id
) {
    UserAccountDoc extracted_user_event(event_oid);

    ASSERT_NE(extracted_user_event.current_object_oid.to_string(), "000000000000000000000000");
    ASSERT_FALSE(extracted_user_event.installation_ids.empty());

    std::vector<bsoncxx::oid> picture_references;
    for(const auto& pic : extracted_user_event.pictures) {
        if(pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            picture_references.emplace_back(picture_reference);
        }
    }

    getFromEventRequestMessage(
            event_request,
            event_oid,
            user_account_type,
            current_timestamp,
            extracted_user_event.installation_ids.front(),
            extracted_user_event.phone_number,
            picture_references,
            event_created_by,
            event_chat_room_id
    );
}

void UserAccountDoc::getFromEventRequestMessage(
        const EventRequestMessage& event_request,
        const bsoncxx::oid& event_oid,
        UserAccountType user_account_type,
        const std::chrono::milliseconds& current_timestamp,
        const std::string& _installation_id,
        const std::string& _phone_number,
        const std::vector<bsoncxx::oid>& picture_references,
        const std::string& event_created_by,
        const std::string& event_chat_room_id
        ) {

    current_object_oid = event_oid;

    const bsoncxx::types::b_date current_timestamp_date{current_timestamp};

    status = UserAccountStatus::STATUS_ACTIVE;
    inactive_message = DEFAULT_STRING;

    inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    number_of_times_timed_out = INT_ZERO;

    installation_ids.clear();
    installation_ids.emplace_back(_installation_id);

    last_verified_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    time_created = current_timestamp_date;

    account_id_list.clear();

    subscription_status = UserSubscriptionStatus::NO_SUBSCRIPTION;
    subscription_expiration_time = DEFAULT_DATE;
    account_type = user_account_type;

    phone_number = _phone_number;
    first_name = event_request.event_title();
    bio = event_request.bio();
    city = event_request.city();
    gender = general_values::EVENT_GENDER_VALUE;
    birth_year = DEFAULT_INT;
    birth_month = DEFAULT_INT;
    birth_day_of_month = DEFAULT_INT;
    birth_day_of_year = DEFAULT_INT;
    age = general_values::EVENT_AGE_VALUE;
    email_address = DEFAULT_STRING;

    email_address_requires_verification = true;
    opted_in_to_promotional_email = false;

    pictures.clear();
    for(const auto& ref : picture_references) {
        pictures.emplace_back(
                PictureReference(
                        ref,
                        current_timestamp_date
                        )
                );
    }

    for (int i = (int)pictures.size(); i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; ++i) {
        pictures.emplace_back(PictureReference());
    }

    search_by_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];


    bsoncxx::builder::basic::array index_numbers_array_value; //used to search for activities
    index_numbers_array_value.append(event_request.activity().activity_index());

    const bsoncxx::array::view index_numbers_array_view = index_numbers_array_value.view();
    static const std::string PROJECTED_ARRAY_NAME = "pAn";

    mongocxx::stdx::optional<mongocxx::cursor> find_activities_cursor;
    try {
        mongocxx::pipeline find_activities;

        find_activities.match(
                document{}
                        << "_id" << activities_info_keys::ID
                        << finalize
        );

        const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                PROJECTED_ARRAY_NAME,
                index_numbers_array_view,
                event_request.min_allowed_age()
        );

        find_activities.project(project_activities_by_index.view());

        find_activities_cursor = activities_info_collection.aggregate(find_activities);
    } catch (const mongocxx::logic_error& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromEventRequestMessage\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return;
    }

    bsoncxx::array::view projected_categories_array_value;

    //should only have 1 element (included a matched stage searching by _id)
    for (const auto& projected_category_doc: *find_activities_cursor) {
        auto categories_array_element = projected_category_doc[PROJECTED_ARRAY_NAME];
        if (categories_array_element
            && categories_array_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
            projected_categories_array_value = categories_array_element.get_array().value;
        } else { //if element does not exist or is not type array
            const std::string error_string = "Element did not exist or was not type array when extracting"
                                             " PROJECTED_ARRAY_NAME.";

            std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                    << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromEventRequestMessage\n"
                    << error_string << '\n'
                    << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
            return;
        }
    }

    int category_index = 0;
    for (const auto& category_doc: projected_categories_array_value) {
        auto category_index_element = category_doc[activities_info_keys::activities::CATEGORY_INDEX];
        if (category_index_element
            && category_index_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            category_index = category_index_element.get_int32().value;
        } else { //if element does not exist or is not type int32
            const std::string error_string = "Element did not exist or was not type array when extracting CATEGORY_INDEX.";

            std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                    << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromEventRequestMessage\n"
                    << error_string << '\n'
                    << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
            return;
        }
    }

    std::vector<TestTimeframe> activity_time_frames;
    activity_time_frames.emplace_back(
            TestTimeframe{
                    event_request.activity().time_frame_array(0).start_time_frame(),
                    1
            }
    );
    activity_time_frames.emplace_back(
            TestTimeframe{
                    event_request.activity().time_frame_array(0).stop_time_frame(),
                    -1
            }
    );

    categories = std::vector<TestCategory>{
            TestCategory(
                    AccountCategoryType::ACTIVITY_TYPE,
                    event_request.activity().activity_index(),
                    activity_time_frames
                    ),
            TestCategory(
                    AccountCategoryType::CATEGORY_TYPE,
                    category_index,
                    activity_time_frames
                    )
    };

    event_values.reset();
    event_values.emplace(
            EventValues{
                    event_created_by,
                    event_chat_room_id,
                    event_request.event_title()
                }
            );

    event_expiration_time =
            bsoncxx::types::b_date{
                    std::chrono::milliseconds{event_request.activity().time_frame_array(0).stop_time_frame()} -
                    matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED
            };

    user_created_events.clear();

    other_accounts_matched_with.clear();

    matching_activated = true;

    location = Location(
            event_request.location_longitude(),
            event_request.location_latitude()
    );

    find_matches_timestamp_lock = DEFAULT_DATE;
    last_time_find_matches_ran = bsoncxx::types::b_date{std::chrono::milliseconds{general_values::EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN}};

    age_range = AgeRange(
            event_request.min_allowed_age(),
            event_request.max_allowed_age()
    );

    genders_range.clear();
    for(auto& gender_msg : event_request.allowed_genders()) {
        genders_range.emplace_back(gender_msg);
    }

    max_distance = server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE;

    previously_matched_accounts.clear();

    last_time_empty_match_returned = DEFAULT_DATE;
    last_time_match_algorithm_ran = DEFAULT_DATE;

    int_for_match_list_to_draw_from = INT_ZERO;
    total_number_matches_drawn = INT_ZERO;

    has_been_extracted_accounts_list.clear();
    algorithm_matched_accounts_list.clear();
    other_users_matched_accounts_list.clear();

    time_sms_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;
    swipes_last_updated_time = current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();

    logged_in_token = DEFAULT_STRING;
    logged_in_token_expiration = DEFAULT_DATE;
    logged_in_installation_id = DEFAULT_STRING;

    cool_down_on_sms_return_message = DEFAULT_INT;
    logged_in_return_message = DEFAULT_INT;

    time_email_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

    last_time_displayed_info_updated = DEFAULT_DATE;

    birthday_timestamp = DEFAULT_DATE;
    gender_timestamp = DEFAULT_DATE;
    first_name_timestamp = DEFAULT_DATE;
    bio_timestamp = DEFAULT_DATE;
    city_name_timestamp = DEFAULT_DATE;
    post_login_info_timestamp = DEFAULT_DATE;
    email_timestamp = DEFAULT_DATE;
    categories_timestamp = DEFAULT_DATE;

    chat_rooms.clear();
    other_users_blocked.clear();

    number_times_swiped_yes = INT_ZERO;
    number_times_swiped_no = INT_ZERO;
    number_times_swiped_block = INT_ZERO;
    number_times_swiped_report = INT_ZERO;

    number_times_others_swiped_yes = INT_ZERO;
    number_times_others_swiped_no = INT_ZERO;
    number_times_others_swiped_block = INT_ZERO;
    number_times_others_swiped_report = INT_ZERO;

    number_times_sent_activity_suggestion = INT_ZERO;
    number_times_sent_bug_report = INT_ZERO;
    number_times_sent_other_suggestion = INT_ZERO;

    number_times_spam_feedback_sent = INT_ZERO;
    number_times_spam_reports_sent = INT_ZERO;

    number_times_reported_by_others_in_chat_room = INT_ZERO;
    number_times_blocked_by_others_in_chat_room = INT_ZERO;
    number_times_this_user_reported_from_chat_room = INT_ZERO;
    number_times_this_user_blocked_from_chat_room = INT_ZERO;

    disciplinary_record.clear();
}

void extractMatchingElementToVector(
        std::vector<MatchingElement>& matching_array,
        const bsoncxx::array::view& matching_array_doc
) {

    matching_array.clear();
    for (const auto& match_element : matching_array_doc) {
        bsoncxx::document::view match_element_doc = match_element.get_document().value;

        matching_array.emplace_back(
                extractMatchingElementToClass(match_element_doc)
        );
    }
}

bool UserAccountDoc::getFromCollection(const std::string& _phoneNumber) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database database = mongo_cpp_client[database_name];
    mongocxx::collection collection = database[collection_name];

    bsoncxx::stdx::optional<bsoncxx::document::value> find_document_val;
    try {
        find_document_val = collection.find_one(
                document{}
                        << user_account_keys::PHONE_NUMBER << _phoneNumber
                        << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(find_document_val);
}

bool UserAccountDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {

    try {
        current_object_oid = user_account_document["_id"].get_oid().value;

        status = UserAccountStatus(user_account_document[user_account_keys::STATUS].get_int32().value);
        inactive_message = user_account_document[user_account_keys::INACTIVE_MESSAGE].get_string().value.to_string();

        auto inactive_end_time_element = user_account_document[user_account_keys::INACTIVE_END_TIME];
        if (inactive_end_time_element.type() == bsoncxx::type::k_null) {
            inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
        } else {
            inactive_end_time = user_account_document[user_account_keys::INACTIVE_END_TIME].get_date();
        }

        number_of_times_timed_out = user_account_document[user_account_keys::NUMBER_OF_TIMES_TIMED_OUT].get_int32().value;

        bsoncxx::array::view installation_ids_doc = user_account_document[user_account_keys::INSTALLATION_IDS].get_array().value;

        installation_ids.clear();
        for (const auto& installation_id : installation_ids_doc) {
            installation_ids.emplace_back(
                    installation_id.get_string().value.to_string()
            );
        }

        last_verified_time = user_account_document[user_account_keys::LAST_VERIFIED_TIME].get_date();
        time_created = user_account_document[user_account_keys::TIME_CREATED].get_date();

        bsoncxx::array::view account_id_list_doc = user_account_document[user_account_keys::ACCOUNT_ID_LIST].get_array().value;
        account_id_list.clear();
        for (const auto& account_id : account_id_list_doc) {
            account_id_list.emplace_back(
                    account_id.get_string().value.to_string()
            );
        }

        subscription_status = UserSubscriptionStatus(user_account_document[user_account_keys::SUBSCRIPTION_STATUS].get_int32().value);
        subscription_expiration_time = user_account_document[user_account_keys::SUBSCRIPTION_EXPIRATION_TIME].get_date();
        account_type = UserAccountType(user_account_document[user_account_keys::ACCOUNT_TYPE].get_int32().value);

        phone_number = user_account_document[user_account_keys::PHONE_NUMBER].get_string().value.to_string();
        first_name = user_account_document[user_account_keys::FIRST_NAME].get_string().value.to_string();
        bio = user_account_document[user_account_keys::BIO].get_string().value.to_string();
        city = user_account_document[user_account_keys::CITY].get_string().value.to_string();
        gender = user_account_document[user_account_keys::GENDER].get_string().value.to_string();
        birth_year = user_account_document[user_account_keys::BIRTH_YEAR].get_int32().value;
        birth_month = user_account_document[user_account_keys::BIRTH_MONTH].get_int32().value;
        birth_day_of_month = user_account_document[user_account_keys::BIRTH_DAY_OF_MONTH].get_int32().value;
        birth_day_of_year = user_account_document[user_account_keys::BIRTH_DAY_OF_YEAR].get_int32().value;
        age = user_account_document[user_account_keys::AGE].get_int32().value;
        email_address = user_account_document[user_account_keys::EMAIL_ADDRESS].get_string().value.to_string();

        email_address_requires_verification = user_account_document[user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION].get_bool().value;
        opted_in_to_promotional_email = user_account_document[user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL].get_bool().value;

        //NOTE: it is important extract null here and put something to represent it to preserve indexing
        bsoncxx::array::view pictures_doc = user_account_document[user_account_keys::PICTURES].get_array().value;
        pictures.clear();
        for (const auto& picture : pictures_doc) {
            if (picture.type() == bsoncxx::type::k_document) {
                bsoncxx::document::view pic_doc = picture.get_document().value;
                pictures.emplace_back(
                        PictureReference(
                                pic_doc[user_account_keys::pictures::OID_REFERENCE].get_oid().value,
                                pic_doc[user_account_keys::pictures::TIMESTAMP_STORED].get_date()
                        )
                );
            } else if (picture.type() == bsoncxx::type::k_null) {
                pictures.emplace_back(PictureReference());
            } else {
                std::cout << "ERROR, Picture type was " + convertBsonTypeToString(picture.type()) +
                             " UserAccountDoc::saveInfoToDocument\n";
                return false;
            }
        }

        search_by_options = AlgorithmSearchOptions(
                user_account_document[user_account_keys::SEARCH_BY_OPTIONS].get_int32().value);

        bsoncxx::array::view categories_doc = user_account_document[user_account_keys::CATEGORIES].get_array().value;
        categories.clear();
        for (const auto& category : categories_doc) {
            bsoncxx::document::view category_doc = category.get_document().value;
            bsoncxx::array::view timeframes_doc = category_doc[user_account_keys::categories::TIMEFRAMES].get_array().value;

            std::vector<TestTimeframe> timeframes;
            for (const auto& time_frame : timeframes_doc) {
                bsoncxx::document::view time_frame_doc = time_frame.get_document().value;
                timeframes.emplace_back(
                        TestTimeframe(
                                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value
                        )
                );
            }

            categories.emplace_back(
                    TestCategory(
                            AccountCategoryType(category_doc[user_account_keys::categories::TYPE].get_int32().value),
                            category_doc[user_account_keys::categories::INDEX_VALUE].get_int32().value,
                            timeframes
                    )
            );
        }

        event_values.reset();
        const auto event_values_ele = user_account_document[user_account_keys::EVENT_VALUES];
        if(event_values_ele) {
            const bsoncxx::document::view event_values_doc = event_values_ele.get_document().value;
            event_values = EventValues(
                    event_values_doc[user_account_keys::event_values::CREATED_BY].get_string().value.to_string(),
                    event_values_doc[user_account_keys::event_values::CHAT_ROOM_ID].get_string().value.to_string(),
                    event_values_doc[user_account_keys::event_values::EVENT_TITLE].get_string().value.to_string()
            );
        }

        event_expiration_time = user_account_document[user_account_keys::EVENT_EXPIRATION_TIME].get_date();

        bsoncxx::array::view user_created_events_doc = user_account_document[user_account_keys::USER_CREATED_EVENTS].get_array().value;
        user_created_events.clear();
        for(const auto& user_created_event : user_created_events_doc) {
            const bsoncxx::document::view user_created_event_doc = user_created_event.get_document().value;

            user_created_events.emplace_back(
                    user_created_event_doc[user_account_keys::user_created_events::EVENT_OID].get_oid().value,
                    user_created_event_doc[user_account_keys::user_created_events::EXPIRATION_TIME].get_date(),
                    LetsGoEventStatus(user_created_event_doc[user_account_keys::user_created_events::EVENT_STATE].get_int32().value)
                );
        }

        bsoncxx::array::view other_accounts_matched_with_doc = user_account_document[user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH].get_array().value;
        other_accounts_matched_with.clear();
        for (const auto& other_account : other_accounts_matched_with_doc) {
            bsoncxx::document::view other_account_doc = other_account.get_document().value;
            other_accounts_matched_with.emplace_back(
                    OtherAccountMatchedWith(
                            other_account_doc[user_account_keys::other_accounts_matched_with::OID_STRING].get_string().value.to_string(),
                            other_account_doc[user_account_keys::other_accounts_matched_with::TIMESTAMP].get_date()
                    )
            );
        }

        matching_activated = user_account_document[user_account_keys::MATCHING_ACTIVATED].get_bool().value;

        bsoncxx::array::view location_array = user_account_document[user_account_keys::LOCATION].get_document().value["coordinates"].get_array().value;

        location = Location(
                location_array[0].get_double().value,
                location_array[1].get_double().value
        );

        find_matches_timestamp_lock = user_account_document[user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK].get_date();
        last_time_find_matches_ran = user_account_document[user_account_keys::LAST_TIME_FIND_MATCHES_RAN].get_date();

        bsoncxx::document::view age_range_doc = user_account_document[user_account_keys::AGE_RANGE].get_document().value;

        age_range = AgeRange(
                age_range_doc[user_account_keys::age_range::MIN].get_int32().value,
                age_range_doc[user_account_keys::age_range::MAX].get_int32().value
        );

        bsoncxx::array::view genders_range_doc = user_account_document[user_account_keys::GENDERS_RANGE].get_array().value;
        genders_range.clear();
        for (const auto& gender_range : genders_range_doc) {
            genders_range.emplace_back(gender_range.get_string().value.to_string());
        }

        max_distance = user_account_document[user_account_keys::MAX_DISTANCE].get_int32().value;

        bsoncxx::array::view previously_matched_accounts_doc = user_account_document[user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS].get_array().value;
        previously_matched_accounts.clear();
        for (const auto& previously_matched_account : previously_matched_accounts_doc) {
            bsoncxx::document::view previously_matched_account_doc = previously_matched_account.get_document().value;
            previously_matched_accounts.emplace_back(
                    PreviouslyMatchedAccounts(
                            previously_matched_account_doc[user_account_keys::previously_matched_accounts::OID].get_oid().value,
                            previously_matched_account_doc[user_account_keys::previously_matched_accounts::TIMESTAMP].get_date(),
                            previously_matched_account_doc[user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED].get_int32().value
                    )
            );
        }

        last_time_empty_match_returned = user_account_document[user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED].get_date();
        last_time_match_algorithm_ran = user_account_document[user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN].get_date();

        int_for_match_list_to_draw_from = user_account_document[user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM].get_int32().value;
        total_number_matches_drawn = user_account_document[user_account_keys::TOTAL_NUMBER_MATCHES_DRAWN].get_int32().value;

        bsoncxx::array::view has_been_extracted_accounts_list_doc = user_account_document[user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST].get_array().value;
        extractMatchingElementToVector(
                has_been_extracted_accounts_list,
                has_been_extracted_accounts_list_doc
        );

        bsoncxx::array::view algorithm_matched_accounts_list_doc = user_account_document[user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST].get_array().value;
        extractMatchingElementToVector(
                algorithm_matched_accounts_list,
                algorithm_matched_accounts_list_doc
        );

        bsoncxx::array::view other_users_matched_accounts_list_doc = user_account_document[user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST].get_array().value;
        extractMatchingElementToVector(
                other_users_matched_accounts_list,
                other_users_matched_accounts_list_doc
        );

        time_sms_can_be_sent_again = user_account_document[user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN].get_date();
        number_swipes_remaining = user_account_document[user_account_keys::NUMBER_SWIPES_REMAINING].get_int32().value;
        swipes_last_updated_time = user_account_document[user_account_keys::SWIPES_LAST_UPDATED_TIME].get_int64().value;

        logged_in_token = user_account_document[user_account_keys::LOGGED_IN_TOKEN].get_string().value.to_string();
        logged_in_token_expiration = user_account_document[user_account_keys::LOGGED_IN_TOKEN_EXPIRATION].get_date();
        logged_in_installation_id = user_account_document[user_account_keys::LOGGED_IN_INSTALLATION_ID].get_string().value.to_string();

        cool_down_on_sms_return_message = user_account_document[user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE].get_int32().value;
        logged_in_return_message = user_account_document[user_account_keys::LOGGED_IN_RETURN_MESSAGE].get_int32().value;

        time_email_can_be_sent_again = user_account_document[user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN].get_date();

        last_time_displayed_info_updated = user_account_document[user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED].get_date();

        birthday_timestamp = user_account_document[user_account_keys::BIRTHDAY_TIMESTAMP].get_date();
        gender_timestamp = user_account_document[user_account_keys::GENDER_TIMESTAMP].get_date();
        first_name_timestamp = user_account_document[user_account_keys::FIRST_NAME_TIMESTAMP].get_date();
        bio_timestamp = user_account_document[user_account_keys::BIO_TIMESTAMP].get_date();
        city_name_timestamp = user_account_document[user_account_keys::CITY_NAME_TIMESTAMP].get_date();
        post_login_info_timestamp = user_account_document[user_account_keys::POST_LOGIN_INFO_TIMESTAMP].get_date();
        email_timestamp = user_account_document[user_account_keys::EMAIL_TIMESTAMP].get_date();
        categories_timestamp = user_account_document[user_account_keys::CATEGORIES_TIMESTAMP].get_date();

        bsoncxx::array::view chat_rooms_doc = user_account_document[user_account_keys::CHAT_ROOMS].get_array().value;
        chat_rooms.clear();
        for (const auto& chat_room : chat_rooms_doc) {
            bsoncxx::document::view chat_room_doc = chat_room.get_document().value;

            const auto event_oid_ele = chat_room_doc[user_account_keys::chat_rooms::EVENT_OID];

            std::optional<bsoncxx::oid> event_oid;
            if(event_oid_ele) {
                event_oid = event_oid_ele.get_oid().value;
            }

            chat_rooms.emplace_back(
                    SingleChatRoom(
                            chat_room_doc[user_account_keys::chat_rooms::CHAT_ROOM_ID].get_string().value.to_string(),
                            chat_room_doc[user_account_keys::chat_rooms::LAST_TIME_VIEWED].get_date(),
                            event_oid
                    )
            );
        }

        bsoncxx::array::view other_users_blocked_doc = user_account_document[user_account_keys::OTHER_USERS_BLOCKED].get_array().value;
        other_users_blocked.clear();
        for (const auto& other_user : other_users_blocked_doc) {
            bsoncxx::document::view other_user_doc = other_user.get_document().value;
            other_users_blocked.emplace_back(
                    OtherBlockedUser(
                            other_user_doc[user_account_keys::other_users_blocked::OID_STRING].get_string().value.to_string(),
                            other_user_doc[user_account_keys::other_users_blocked::TIMESTAMP_BLOCKED].get_date()
                    )
            );
        }

        number_times_swiped_yes = user_account_document[user_account_keys::NUMBER_TIMES_SWIPED_YES].get_int32().value;
        number_times_swiped_no = user_account_document[user_account_keys::NUMBER_TIMES_SWIPED_NO].get_int32().value;
        number_times_swiped_block = user_account_document[user_account_keys::NUMBER_TIMES_SWIPED_BLOCK].get_int32().value;
        number_times_swiped_report = user_account_document[user_account_keys::NUMBER_TIMES_SWIPED_REPORT].get_int32().value;

        number_times_others_swiped_yes = user_account_document[user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES].get_int32().value;
        number_times_others_swiped_no = user_account_document[user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO].get_int32().value;
        number_times_others_swiped_block = user_account_document[user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK].get_int32().value;
        number_times_others_swiped_report = user_account_document[user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT].get_int32().value;

        number_times_sent_activity_suggestion = user_account_document[user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION].get_int32().value;
        number_times_sent_bug_report = user_account_document[user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT].get_int32().value;
        number_times_sent_other_suggestion = user_account_document[user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION].get_int32().value;

        number_times_spam_feedback_sent = user_account_document[user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT].get_int32().value;
        number_times_spam_reports_sent = user_account_document[user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT].get_int32().value;

        number_times_reported_by_others_in_chat_room = user_account_document[user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM].get_int32().value;
        number_times_blocked_by_others_in_chat_room = user_account_document[user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM].get_int32().value;
        number_times_this_user_reported_from_chat_room = user_account_document[user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM].get_int32().value;
        number_times_this_user_blocked_from_chat_room = user_account_document[user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM].get_int32().value;

        bsoncxx::array::view disciplinary_record_doc = user_account_document[user_account_keys::DISCIPLINARY_RECORD].get_array().value;
        disciplinary_record.clear();
        for (const auto& single_disciplinary_record : disciplinary_record_doc) {
            bsoncxx::document::view single_disciplinary_record_doc = single_disciplinary_record.get_document().value;
            disciplinary_record.emplace_back(
                    DisciplinaryRecord(
                            single_disciplinary_record_doc[user_account_keys::disciplinary_record::SUBMITTED_TIME].get_date(),
                            single_disciplinary_record_doc[user_account_keys::disciplinary_record::END_TIME].get_date(),
                            DisciplinaryActionTypeEnum(
                                    single_disciplinary_record_doc[user_account_keys::disciplinary_record::ACTION_TYPE].get_int32().value),
                            single_disciplinary_record_doc[user_account_keys::disciplinary_record::REASON].get_string().value.to_string(),
                            single_disciplinary_record_doc[user_account_keys::disciplinary_record::ADMIN_NAME].get_string().value.to_string()
                    )
            );
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::convertDocumentToClass\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool UserAccountDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {

    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }

}

std::ostream& operator<<(std::ostream& o, const UserAccountDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}

void checkMatchingElementVectorEquality(
        const std::vector<MatchingElement>& current_user_matched_with,
        const std::vector<MatchingElement>& other_user_matched_with,
        const std::string& key,
        const std::string& object_class_name,
        bool& return_value
) {
    if (current_user_matched_with.size() == other_user_matched_with.size()) {
        for (size_t i = 0; i < current_user_matched_with.size(); i++) {
            checkMatchingElementEquality(
                    current_user_matched_with[i],
                    other_user_matched_with[i],
                    key,
                    object_class_name,
                    return_value
            );
        }
    } else {
        checkForEquality(
                current_user_matched_with.size(),
                other_user_matched_with.size(),
                key + ".size()",
                object_class_name,
                return_value
        );
    }
}

void checkMatchingElementEquality(
        const MatchingElement& first_matching_element,
        const MatchingElement& other_matching_element,
        const std::string& key,
        const std::string& object_class_name,
        bool& return_value
) {
    checkForEquality(
            first_matching_element.oid.to_string(),
            other_matching_element.oid.to_string(),
            key + ".oid",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.point_value,
            other_matching_element.point_value,
            key + ".point_value",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.distance,
            other_matching_element.distance,
            key + ".distance",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.expiration_time.value.count(),
            other_matching_element.expiration_time.value.count(),
            key + ".expiration_time",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.match_timestamp.value.count(),
            other_matching_element.match_timestamp.value.count(),
            key + ".match_timestamp",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.from_match_algorithm_list,
            other_matching_element.from_match_algorithm_list,
            key + ".from_match_algorithm_list",
            object_class_name,
            return_value
    );

    checkForEquality(
            first_matching_element.saved_statistics_oid.to_string(),
            other_matching_element.saved_statistics_oid.to_string(),
            key + ".saved_statistics_oid",
            object_class_name,
            return_value
    );
}

bool UserAccountDoc::operator==(const UserAccountDoc& other) const {

    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            status,
            other.status,
            "STATUS",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            inactive_message,
            other.inactive_message,
            "INACTIVE_MESSAGE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            inactive_end_time.value.count(),
            other.inactive_end_time.value.count(),
            "INACTIVE_END_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_of_times_timed_out,
            other.number_of_times_timed_out,
            "NUMBER_OF_TIMES_TIMED_OUT",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.installation_ids.size() == installation_ids.size()) {
        for (size_t i = 0; i < installation_ids.size(); i++) {
            checkForEquality(
                    installation_ids[i],
                    other.installation_ids[i],
                    "INSTALLATION_IDS",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                installation_ids.size(),
                other.installation_ids.size(),
                "INSTALLATION_IDS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            last_verified_time.value.count(),
            other.last_verified_time.value.count(),
            "LAST_VERIFIED_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            time_created.value.count(),
            other.time_created.value.count(),
            "TIME_CREATED",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.account_id_list.size() == account_id_list.size()) {
        for (size_t i = 0; i < account_id_list.size(); i++) {
            checkForEquality(
                    account_id_list[i],
                    other.account_id_list[i],
                    "ACCOUNT_ID_LIST",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                account_id_list.size(),
                other.account_id_list.size(),
                "ACCOUNT_ID_LIST.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            subscription_status,
            other.subscription_status,
            "SUBSCRIPTION_STATUS",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            subscription_expiration_time.value.count(),
            other.subscription_expiration_time.value.count(),
            "SUBSCRIPTION_EXPIRATION_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            account_type,
            other.account_type,
            "ACCOUNT_TYPE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            phone_number,
            other.phone_number,
            "PHONE_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            first_name,
            other.first_name,
            "FIRST_NAME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            bio,
            other.bio,
            "BIO",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            city,
            other.city,
            "CITY",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            gender,
            other.gender,
            "GENDER",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            birth_year,
            other.birth_year,
            "BIRTH_YEAR",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            birth_month,
            other.birth_month,
            "BIRTH_MONTH",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            birth_day_of_month,
            other.birth_day_of_month,
            "BIRTH_DAY_OF_MONTH",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            birth_day_of_year,
            other.birth_day_of_year,
            "BIRTH_DAY_OF_YEAR",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            age,
            other.age,
            "AGE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            email_address,
            other.email_address,
            "EMAIL_ADDRESS",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            email_address_requires_verification,
            other.email_address_requires_verification,
            "EMAIL_ADDRESS_REQUIRES_VERIFICATION",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            opted_in_to_promotional_email,
            other.opted_in_to_promotional_email,
            "OPTED_IN_TO_PROMOTIONAL_EMAIL",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.pictures.size() == pictures.size()) {
        for (int i = 0; i < (int)pictures.size(); i++) {
            checkForEquality(
                    pictures[i],
                    other.pictures[i],
                    "PICTURES",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                pictures.size(),
                other.pictures.size(),
                "PICTURES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            search_by_options,
            other.search_by_options,
            "SEARCH_BY_OPTIONS",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.categories.size() == categories.size()) {
        for (size_t i = 0; i < categories.size(); i++) {

            checkForEquality(
                    categories[i].type,
                    other.categories[i].type,
                    "CATEGORIES.type",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].index_value,
                    other.categories[i].index_value,
                    "CATEGORIES.index_value",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if (other.categories[i].time_frames.size() == categories[i].time_frames.size()) {
                for (size_t j = 0; j < categories[i].time_frames.size(); j++) {
                    checkForEquality(
                            categories[i].time_frames[j].startStopValue,
                            other.categories[i].time_frames[j].startStopValue,
                            "CATEGORIES.timeframes.startStopValue",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            categories[i].time_frames[j].time,
                            other.categories[i].time_frames[j].time,
                            "CATEGORIES.timeframes.time",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                }
            } else {
                checkForEquality(
                        categories[i].time_frames.size(),
                        other.categories[i].time_frames.size(),
                        "CATEGORIES.timeframes.size()",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }

        }
    } else {
        checkForEquality(
                categories.size(),
                other.categories.size(),
                "CATEGORIES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.event_values && event_values) {
        checkForEquality(
                event_values->created_by,
                other.event_values->created_by,
                "EVENT_VALUES.CREATED_BY",
                OBJECT_CLASS_NAME,
                return_value
        );

        checkForEquality(
                event_values->chat_room_id,
                other.event_values->chat_room_id,
                "EVENT_VALUES.CHAT_ROOM_ID",
                OBJECT_CLASS_NAME,
                return_value
        );

        checkForEquality(
                event_values->event_title,
                other.event_values->event_title,
                "EVENT_VALUES.EVENT_TITLE",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                event_values.operator bool(),
                other.event_values.operator bool(),
                "EVENT_VALUES existing",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            event_expiration_time,
            other.event_expiration_time,
            "EVENT_EXPIRATION_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.user_created_events.size() == user_created_events.size()) {
        for (int i = 0; i < (int) user_created_events.size(); i++) {
            checkForEquality(
                    user_created_events[i].event_oid.to_string(),
                    other.user_created_events[i].event_oid.to_string(),
                    "USER_CREATED_EVENTS.EVENT_OID",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    user_created_events[i].expiration_time,
                    other.user_created_events[i].expiration_time,
                    "USER_CREATED_EVENTS.EXPIRATION_TIME",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    user_created_events[i].event_state,
                    other.user_created_events[i].event_state,
                    "USER_CREATED_EVENTS.EVENT_STATE",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                user_created_events.size(),
                other.user_created_events.size(),
                "USER_CREATED_EVENTS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.other_accounts_matched_with.size() == other_accounts_matched_with.size()) {
        for (int i = 0; i < (int)other_accounts_matched_with.size(); i++) {
            checkForEquality(
                    other_accounts_matched_with[i].oid_string,
                    other.other_accounts_matched_with[i].oid_string,
                    "OTHER_ACCOUNTS_MATCHED_WITH.oid_string",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    other_accounts_matched_with[i].timestamp,
                    other.other_accounts_matched_with[i].timestamp,
                    "OTHER_ACCOUNTS_MATCHED_WITH.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                other_accounts_matched_with.size(),
                other.other_accounts_matched_with.size(),
                "OTHER_ACCOUNTS_MATCHED_WITH.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            matching_activated,
            other.matching_activated,
            "MATCHING_ACTIVATED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            location.longitude,
            other.location.longitude,
            "LOCATION.longitude",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            location.latitude,
            other.location.latitude,
            "LOCATION.latitude",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            find_matches_timestamp_lock.value.count(),
            other.find_matches_timestamp_lock.value.count(),
            "FIND_MATCHES_TIMESTAMP_LOCK",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            last_time_find_matches_ran.value.count(),
            other.last_time_find_matches_ran.value.count(),
            "LAST_TIME_FIND_MATCHES_RAN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            age_range.min,
            other.age_range.min,
            "AGE_RANGE.MIN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            age_range.max,
            other.age_range.max,
            "AGE_RANGE.MAX",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.genders_range.size() == genders_range.size()) {
        for (int i = 0; i < (int)genders_range.size(); i++) {
            checkForEquality(
                    genders_range[i],
                    other.genders_range[i],
                    "GENDERS_RANGE",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                genders_range.size(),
                other.genders_range.size(),
                "GENDERS_RANGE.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            max_distance,
            other.max_distance,
            "MAX_DISTANCE",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.previously_matched_accounts.size() == previously_matched_accounts.size()) {
        for (int i = 0; i < (int)previously_matched_accounts.size(); i++) {
            checkForEquality(
                    previously_matched_accounts[i].oid.to_string(),
                    other.previously_matched_accounts[i].oid.to_string(),
                    "PREVIOUSLY_MATCHED_ACCOUNTS.oid",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    previously_matched_accounts[i].timestamp.value.count(),
                    other.previously_matched_accounts[i].timestamp.value.count(),
                    "PREVIOUSLY_MATCHED_ACCOUNTS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    previously_matched_accounts[i].number_times_matched,
                    other.previously_matched_accounts[i].number_times_matched,
                    "PREVIOUSLY_MATCHED_ACCOUNTS.number_times_matched",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                previously_matched_accounts.size(),
                other.previously_matched_accounts.size(),
                "PREVIOUSLY_MATCHED_ACCOUNTS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            last_time_empty_match_returned.value.count(),
            other.last_time_empty_match_returned.value.count(),
            "LAST_TIME_EMPTY_MATCH_RETURNED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            last_time_match_algorithm_ran.value.count(),
            other.last_time_match_algorithm_ran.value.count(),
            "LAST_TIME_MATCH_ALGORITHM_RAN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            int_for_match_list_to_draw_from,
            other.int_for_match_list_to_draw_from,
            "INT_FOR_MATCH_LIST_TO_DRAW_FROM",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            total_number_matches_drawn,
            other.total_number_matches_drawn,
            "TOTAL_NUMBER_MATCHES_DRAWN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkMatchingElementVectorEquality(
            has_been_extracted_accounts_list,
            other.has_been_extracted_accounts_list,
            "HAS_BEEN_EXTRACTED_ACCOUNTS_LIST",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkMatchingElementVectorEquality(
            algorithm_matched_accounts_list,
            other.algorithm_matched_accounts_list,
            "ALGORITHM_MATCHED_ACCOUNTS_LIST",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkMatchingElementVectorEquality(
            other_users_matched_accounts_list,
            other.other_users_matched_accounts_list,
            "OTHER_USERS_MATCHED_ACCOUNTS_LIST",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            time_sms_can_be_sent_again.value.count(),
            other.time_sms_can_be_sent_again.value.count(),
            "TIME_SMS_CAN_BE_SENT_AGAIN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_swipes_remaining,
            other.number_swipes_remaining,
            "NUMBER_SWIPES_REMAINING",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            swipes_last_updated_time,
            other.swipes_last_updated_time,
            "SWIPES_LAST_UPDATED_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            logged_in_token,
            other.logged_in_token,
            "LOGGED_IN_TOKEN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            logged_in_token_expiration.value.count(),
            other.logged_in_token_expiration.value.count(),
            "LOGGED_IN_TOKEN_EXPIRATION",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            logged_in_installation_id,
            other.logged_in_installation_id,
            "LOGGED_IN_INSTALLATION_ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    //Temporary values
//    checkForEquality(
//            cool_down_on_sms_return_message,
//            other.cool_down_on_sms_return_message,
//            "COOL_DOWN_ON_SMS_RETURN_MESSAGE",
//            OBJECT_CLASS_NAME,
//            return_value
//    );
//
//    checkForEquality(
//            logged_in_return_message,
//            other.logged_in_return_message,
//            "LOGGED_IN_RETURN_MESSAGE",
//            OBJECT_CLASS_NAME,
//            return_value
//    );

    checkForEquality(
            time_email_can_be_sent_again,
            other.time_email_can_be_sent_again,
            "TIME_EMAIL_CAN_BE_SENT_AGAIN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            last_time_displayed_info_updated,
            other.last_time_displayed_info_updated,
            "LAST_TIME_DISPLAYED_INFO_UPDATED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            birthday_timestamp,
            other.birthday_timestamp,
            "BIRTHDAY_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            gender_timestamp,
            other.gender_timestamp,
            "GENDER_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            first_name_timestamp,
            other.first_name_timestamp,
            "FIRST_NAME_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            bio_timestamp,
            other.bio_timestamp,
            "BIO_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            city_name_timestamp,
            other.city_name_timestamp,
            "CITY_NAME_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            post_login_info_timestamp,
            other.post_login_info_timestamp,
            "POST_LOGIN_INFO_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            email_timestamp,
            other.email_timestamp,
            "EMAIL_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            categories_timestamp,
            other.categories_timestamp,
            "CATEGORIES_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.chat_rooms.size() == chat_rooms.size()) {
        for (int i = 0; i < (int)chat_rooms.size(); i++) {
            checkForEquality(
                    chat_rooms[i].chat_room_id,
                    other.chat_rooms[i].chat_room_id,
                    "CHAT_ROOMS.chat_room_id",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    chat_rooms[i].last_time_viewed.value.count(),
                    other.chat_rooms[i].last_time_viewed.value.count(),
                    "CHAT_ROOMS.last_time_viewed",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if(chat_rooms[i].event_oid && other.chat_rooms[i].event_oid) {
                checkForEquality(
                        chat_rooms[i].event_oid->to_string(),
                        other.chat_rooms[i].event_oid->to_string(),
                        "CHAT_ROOMS.EVENT_OID",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            } else {
                checkForEquality(
                        chat_rooms[i].event_oid.operator bool(),
                        other.chat_rooms[i].event_oid.operator bool(),
                        "CHAT_ROOMS.EVENT_OID existence",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }
        }
    } else {
        checkForEquality(
                chat_rooms.size(),
                other.chat_rooms.size(),
                "CHAT_ROOMS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.other_users_blocked.size() == other_users_blocked.size()) {
        for (int i = 0; i < (int)other_users_blocked.size(); i++) {
            checkForEquality(
                    other_users_blocked[i].oid_string,
                    other.other_users_blocked[i].oid_string,
                    "OTHER_USERS_BLOCKED.oid_string",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    other_users_blocked[i].timestamp_blocked.value.count(),
                    other.other_users_blocked[i].timestamp_blocked.value.count(),
                    "OTHER_USERS_BLOCKED.timestamp_blocked",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                other_users_blocked.size(),
                other.other_users_blocked.size(),
                "OTHER_USERS_BLOCKED.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            number_times_swiped_yes,
            other.number_times_swiped_yes,
            "NUMBER_TIMES_SWIPED_YES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_swiped_no,
            other.number_times_swiped_no,
            "NUMBER_TIMES_SWIPED_NO",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_swiped_block,
            other.number_times_swiped_block,
            "NUMBER_TIMES_SWIPED_BLOCK",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_swiped_report,
            other.number_times_swiped_report,
            "NUMBER_TIMES_SWIPED_REPORT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_others_swiped_yes,
            other.number_times_others_swiped_yes,
            "NUMBER_TIMES_OTHERS_SWIPED_YES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_others_swiped_no,
            other.number_times_others_swiped_no,
            "NUMBER_TIMES_OTHERS_SWIPED_NO",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_others_swiped_block,
            other.number_times_others_swiped_block,
            "NUMBER_TIMES_OTHERS_SWIPED_BLOCK",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_others_swiped_report,
            other.number_times_others_swiped_report,
            "NUMBER_TIMES_OTHERS_SWIPED_REPORT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_sent_activity_suggestion,
            other.number_times_sent_activity_suggestion,
            "NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_sent_bug_report,
            other.number_times_sent_bug_report,
            "NUMBER_TIMES_SENT_BUG_REPORT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_sent_other_suggestion,
            other.number_times_sent_other_suggestion,
            "NUMBER_TIMES_SENT_OTHER_SUGGESTION",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_spam_feedback_sent,
            other.number_times_spam_feedback_sent,
            "NUMBER_TIMES_SPAM_FEEDBACK_SENT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_spam_reports_sent,
            other.number_times_spam_reports_sent,
            "NUMBER_TIMES_SPAM_REPORTS_SENT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_reported_by_others_in_chat_room,
            other.number_times_reported_by_others_in_chat_room,
            "NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_blocked_by_others_in_chat_room,
            other.number_times_blocked_by_others_in_chat_room,
            "NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_this_user_reported_from_chat_room,
            other.number_times_this_user_reported_from_chat_room,
            "NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_times_this_user_blocked_from_chat_room,
            other.number_times_this_user_blocked_from_chat_room,
            "NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.disciplinary_record.size() == disciplinary_record.size()) {
        for (int i = 0; i < (int)disciplinary_record.size(); i++) {
            checkForEquality(
                    disciplinary_record[i].reason,
                    other.disciplinary_record[i].reason,
                    "DISCIPLINARY_RECORD.reason",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    disciplinary_record[i].admin_name,
                    other.disciplinary_record[i].admin_name,
                    "DISCIPLINARY_RECORD.admin_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    disciplinary_record[i].action_type,
                    other.disciplinary_record[i].action_type,
                    "DISCIPLINARY_RECORD.action_type",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    disciplinary_record[i].end_time.value.count(),
                    other.disciplinary_record[i].end_time.value.count(),
                    "DISCIPLINARY_RECORD.end_time",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    disciplinary_record[i].submitted_time.value.count(),
                    other.disciplinary_record[i].submitted_time.value.count(),
                    "DISCIPLINARY_RECORD.submitted_time",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                disciplinary_record.size(),
                other.disciplinary_record.size(),
                "DISCIPLINARY_RECORD.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}
