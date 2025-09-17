//
// Created by jeremiah on 5/31/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <user_account_statistics_keys.h>
#include <user_account_keys.h>
#include <utility_general_functions.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserAccountStatisticsDoc object to a document and saves it to the passed builder
void UserAccountStatisticsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::basic::array login_times_arr;
    bsoncxx::builder::basic::array times_match_occurred_arr;
    bsoncxx::builder::basic::array locations_arr;
    bsoncxx::builder::basic::array phone_numbers_arr;
    bsoncxx::builder::basic::array names_arr;
    bsoncxx::builder::basic::array bios_arr;
    bsoncxx::builder::basic::array cities_arr;
    bsoncxx::builder::basic::array genders_arr;
    bsoncxx::builder::basic::array birth_info_arr;
    bsoncxx::builder::basic::array email_addresses_arr;
    bsoncxx::builder::basic::array categories_arr;
    bsoncxx::builder::basic::array age_ranges_arr;
    bsoncxx::builder::basic::array gender_ranges_arr;
    bsoncxx::builder::basic::array max_distances_arr;
    bsoncxx::builder::basic::array sms_sent_times_arr;
    bsoncxx::builder::basic::array email_sent_times_arr;
    bsoncxx::builder::basic::array emails_verified_arr;
    bsoncxx::builder::basic::array account_recovery_times_arr;
    bsoncxx::builder::basic::array account_logged_out_times_arr;
    bsoncxx::builder::basic::array account_search_by_options_arr;
    bsoncxx::builder::basic::array opted_in_to_promotional_email_arr;

    for (const LoginTime& login_time: login_times) {
        login_times_arr.append(
                document{}
                        << user_account_statistics_keys::login_times::INSTALLATION_ID << login_time.installation_id
                        << user_account_statistics_keys::login_times::DEVICE_NAME << login_time.device_name
                        << user_account_statistics_keys::login_times::API_NUMBER << login_time.api_number
                        << user_account_statistics_keys::login_times::LETS_GO_VERSION << login_time.lets_go_version
                        << user_account_statistics_keys::login_times::TIMESTAMP << login_time.timestamp
                        << finalize
        );
    }

    for (const TimesMatchOccurred& time_match_occurred: times_match_occurred) {
        document times_doc;

        times_doc
                << user_account_statistics_keys::times_match_occurred::MATCHED_OID << time_match_occurred.matched_oid
                << user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID << time_match_occurred.chat_room_id;

        if(time_match_occurred.match_type) {
            times_doc
                    << user_account_statistics_keys::times_match_occurred::MATCH_TYPE << *time_match_occurred.match_type;
        }

        times_doc
            << user_account_statistics_keys::times_match_occurred::TIMESTAMP << time_match_occurred.timestamp;

        times_match_occurred_arr.append(times_doc.view());
    }

    for (const Location& location: locations) {

        bsoncxx::builder::basic::array location_arr;
        location_arr.append(location.location[0]);
        location_arr.append(location.location[1]);

        locations_arr.append(
                document{}
                        << user_account_statistics_keys::locations::LOCATION << location_arr
                        << user_account_statistics_keys::locations::TIMESTAMP << location.timestamp
                        << finalize
        );
    }

    for (const PhoneNumber& phone_number: phone_numbers) {
        phone_numbers_arr.append(
                document{}
                        << user_account_statistics_keys::phone_numbers::PHONE_NUMBER << phone_number.phone_number
                        << user_account_statistics_keys::phone_numbers::TIMESTAMP << phone_number.timestamp
                        << finalize
        );
    }

    for (const Name& name: names) {
        names_arr.append(
                document{}
                        << user_account_statistics_keys::names::NAME << name.name
                        << user_account_statistics_keys::names::TIMESTAMP << name.timestamp
                        << finalize
        );
    }

    for (const Bio& bio: bios) {
        bios_arr.append(
                document{}
                        << user_account_statistics_keys::bios::BIOS << bio.bio
                        << user_account_statistics_keys::bios::TIMESTAMP << bio.timestamp
                        << finalize
        );
    }

    for (const City& city: cities) {
        cities_arr.append(
                document{}
                        << user_account_statistics_keys::cities::CITY << city.city
                        << user_account_statistics_keys::cities::TIMESTAMP << city.timestamp
                        << finalize
        );
    }

    for (const Gender& gender: genders) {
        genders_arr.append(
                document{}
                        << user_account_statistics_keys::genders::GENDER << gender.gender
                        << user_account_statistics_keys::genders::TIMESTAMP << gender.timestamp
                        << finalize
        );
    }

    for (const BirthInfo& single_birth_info: birth_info) {
        birth_info_arr.append(
                document{}
                        << user_account_statistics_keys::birth_info::YEAR << single_birth_info.year
                        << user_account_statistics_keys::birth_info::MONTH << single_birth_info.month
                        << user_account_statistics_keys::birth_info::DAY_OF_MONTH << single_birth_info.day_of_month
                        << user_account_statistics_keys::birth_info::TIMESTAMP << single_birth_info.timestamp
                        << finalize
        );
    }

    for (const EmailAddress& email_address: email_addresses) {
        email_addresses_arr.append(
                document{}
                        << user_account_statistics_keys::email_addresses::EMAIL << email_address.email
                        << user_account_statistics_keys::email_addresses::TIMESTAMP << email_address.timestamp
                        << finalize
        );
    }

    for (const StoredCategories& category : categories) {

        bsoncxx::builder::basic::array stored_categories_arr;

        for (const TestCategory& test_category : category.categories) {
            bsoncxx::builder::basic::array timeframes_doc;

            for (const TestTimeframe& time_frame : test_category.time_frames) {
                timeframes_doc.append(
                        document{}
                                << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                                time_frame.time}
                                << user_account_keys::categories::timeframes::START_STOP_VALUE << time_frame.startStopValue
                                << finalize
                );
            }

            stored_categories_arr.append(
                    document{}
                            << user_account_keys::categories::TYPE << test_category.type
                            << user_account_keys::categories::INDEX_VALUE << test_category.index_value
                            << user_account_keys::categories::TIMEFRAMES << timeframes_doc
                            << finalize
            );
        }

        categories_arr.append(
                document{}
                        << user_account_statistics_keys::categories::ACTIVITIES_AND_CATEGORIES << stored_categories_arr
                        << user_account_statistics_keys::categories::TIMESTAMP << category.timestamp
                        << finalize
        );
    }

    for (const AgeRange& age_range: age_ranges) {

        bsoncxx::builder::basic::array age_range_arr;
        age_range_arr.append(age_range.age_range[0]);
        age_range_arr.append(age_range.age_range[1]);

        age_ranges_arr.append(
                document{}
                        << user_account_statistics_keys::age_ranges::AGE_RANGE << age_range_arr
                        << user_account_statistics_keys::age_ranges::TIMESTAMP << age_range.timestamp
                        << finalize
        );
    }

    for (const GenderRange& gender_range: gender_ranges) {

        bsoncxx::builder::basic::array gender_range_arr;

        for (const std::string& single_gender_range : gender_range.gender_range) {
            gender_range_arr.append(single_gender_range);
        }

        gender_ranges_arr.append(
                document{}
                        << user_account_statistics_keys::gender_ranges::GENDER_RANGE << gender_range_arr
                        << user_account_statistics_keys::gender_ranges::TIMESTAMP << gender_range.timestamp
                        << finalize
        );
    }

    for (const MaxDistance& max_distance: max_distances) {
        max_distances_arr.append(
                document{}
                        << user_account_statistics_keys::max_distances::MAX_DISTANCE << max_distance.max_distance
                        << user_account_statistics_keys::max_distances::TIMESTAMP << max_distance.timestamp
                        << finalize
        );
    }

    for (const bsoncxx::types::b_date& sms_sent_time: sms_sent_times) {
        sms_sent_times_arr.append(sms_sent_time);
    }

    for (const EmailSentTime& email_sent_time: email_sent_times) {
        email_sent_times_arr.append(
                document{}
                        << user_account_statistics_keys::email_sent_times::EMAIL_ADDRESS << email_sent_time.email_address
                        << user_account_statistics_keys::email_sent_times::EMAIL_PREFIX << email_sent_time.email_prefix
                        << user_account_statistics_keys::email_sent_times::EMAIL_SUBJECT << email_sent_time.email_subject
                        << user_account_statistics_keys::email_sent_times::EMAIL_CONTENT << email_sent_time.email_content
                        << user_account_statistics_keys::email_sent_times::TIMESTAMP << email_sent_time.timestamp
                        << finalize
        );
    }

    for (const EmailVerified& email_verified: emails_verified) {
        emails_verified_arr.append(
                document{}
                        << user_account_statistics_keys::emails_verified::EMAILS_VERIFIED_EMAIL << email_verified.emails_verified_email
                        << user_account_statistics_keys::emails_verified::TIMESTAMP << email_verified.timestamp
                        << finalize
        );
    }

    for (const AccountRecoveryTime& account_recovery_time: account_recovery_times) {
        account_recovery_times_arr.append(
                document{}
                        << user_account_statistics_keys::account_recovery_times::PREVIOUS_PHONE_NUMBER << account_recovery_time.previous_phone_number
                        << user_account_statistics_keys::account_recovery_times::NEW_PHONE_NUMBER << account_recovery_time.new_phone_number
                        << user_account_statistics_keys::account_recovery_times::TIMESTAMP << account_recovery_time.timestamp
                        << finalize
        );
    }

    for (const bsoncxx::types::b_date& logged_out_time: account_logged_out_times) {
        account_logged_out_times_arr.append(logged_out_time);
    }

    for (const SearchByOption& search_by_options: account_search_by_options) {
        account_search_by_options_arr.append(
                document{}
                        << user_account_statistics_keys::search_by_options::NEW_SEARCH_BY_OPTIONS << search_by_options.new_search_by_options
                        << user_account_statistics_keys::search_by_options::TIMESTAMP << search_by_options.timestamp
                        << finalize
        );
    }

    for (const OptedInToPromotionalEmail& opted_in_to_promotional_email_single: opted_in_to_promotional_email) {
        opted_in_to_promotional_email_arr.append(
                document{}
                        << user_account_statistics_keys::opted_in_to_promotional_email::NEW_OPTED_IN_TO_PROMOTIONAL_EMAIL << opted_in_to_promotional_email_single.new_opted_in_to_promotional_email
                        << user_account_statistics_keys::opted_in_to_promotional_email::TIMESTAMP << opted_in_to_promotional_email_single.timestamp
                << finalize
        );
    }

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }

    document_result
            << user_account_statistics_keys::LOGIN_TIMES << login_times_arr
            << user_account_statistics_keys::TIMES_MATCH_OCCURRED << times_match_occurred_arr
            << user_account_statistics_keys::LOCATIONS << locations_arr
            << user_account_statistics_keys::PHONE_NUMBERS << phone_numbers_arr
            << user_account_statistics_keys::NAMES << names_arr
            << user_account_statistics_keys::BIOS << bios_arr
            << user_account_statistics_keys::CITIES << cities_arr
            << user_account_statistics_keys::GENDERS << genders_arr
            << user_account_statistics_keys::BIRTH_INFO << birth_info_arr
            << user_account_statistics_keys::EMAIL_ADDRESSES << email_addresses_arr
            << user_account_statistics_keys::CATEGORIES << categories_arr
            << user_account_statistics_keys::AGE_RANGES << age_ranges_arr
            << user_account_statistics_keys::GENDER_RANGES << gender_ranges_arr
            << user_account_statistics_keys::MAX_DISTANCES << max_distances_arr
            << user_account_statistics_keys::SMS_SENT_TIMES << sms_sent_times_arr
            << user_account_statistics_keys::EMAIL_SENT_TIMES << email_sent_times_arr
            << user_account_statistics_keys::EMAILS_VERIFIED << emails_verified_arr
            << user_account_statistics_keys::ACCOUNT_RECOVERY_TIMES << account_recovery_times_arr
            << user_account_statistics_keys::ACCOUNT_LOGGED_OUT_TIMES << account_logged_out_times_arr
            << user_account_statistics_keys::ACCOUNT_SEARCH_BY_OPTIONS << account_search_by_options_arr
            << user_account_statistics_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL << opted_in_to_promotional_email_arr;

}

bool UserAccountStatisticsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {
            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            user_pictures_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {
            auto idVar = user_pictures_collection.insert_one(insertDocument.view());

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


bool UserAccountStatisticsDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = user_pictures_collection.find_one(document{}
                                                                    << "_id" << findOID
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

    return saveInfoToDocument(findDocumentVal);
}

bool UserAccountStatisticsDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val
) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        return false;
    }
}

bool UserAccountStatisticsDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {

        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        const bsoncxx::array::view login_times_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::LOGIN_TIMES
        );

        const bsoncxx::array::view times_match_occurred_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::TIMES_MATCH_OCCURRED
        );

        const bsoncxx::array::view locations_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::LOCATIONS
        );

        const bsoncxx::array::view phone_numbers_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::PHONE_NUMBERS
        );

        const bsoncxx::array::view names_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::NAMES
        );

        const bsoncxx::array::view bios_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::BIOS
        );

        const bsoncxx::array::view cities_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::CITIES
        );

        const bsoncxx::array::view genders_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::GENDERS
        );

        const bsoncxx::array::view birth_info_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::BIRTH_INFO
        );

        const bsoncxx::array::view email_addresses_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::EMAIL_ADDRESSES
        );

        const bsoncxx::array::view categories_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::CATEGORIES
        );

        const bsoncxx::array::view age_ranges_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::AGE_RANGES
        );

        const bsoncxx::array::view gender_ranges_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::GENDER_RANGES
        );

        const bsoncxx::array::view max_distances_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::MAX_DISTANCES
        );

        const bsoncxx::array::view sms_sent_times_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::SMS_SENT_TIMES
        );

        const bsoncxx::array::view email_sent_times_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::EMAIL_SENT_TIMES
        );

        const bsoncxx::array::view emails_verified_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::EMAILS_VERIFIED
        );

        const bsoncxx::array::view account_recovery_times_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::ACCOUNT_RECOVERY_TIMES
        );

        const bsoncxx::array::view account_logged_out_times_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::ACCOUNT_LOGGED_OUT_TIMES
        );

        const bsoncxx::array::view account_search_by_options_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::ACCOUNT_SEARCH_BY_OPTIONS
        );

        const bsoncxx::array::view opted_in_to_promotional_email_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_account_statistics_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL
        );

        login_times.clear();
        for (const auto& ele : login_times_arr) {
            login_times.emplace_back(
                    LoginTime(
                            ele[user_account_statistics_keys::login_times::INSTALLATION_ID].get_string().value.to_string(),
                            ele[user_account_statistics_keys::login_times::DEVICE_NAME].get_string().value.to_string(),
                            ele[user_account_statistics_keys::login_times::API_NUMBER].get_int32().value,
                            ele[user_account_statistics_keys::login_times::LETS_GO_VERSION].get_int32().value,
                            ele[user_account_statistics_keys::login_times::TIMESTAMP].get_date()
                    )
            );
        }

        times_match_occurred.clear();
        for (const auto& ele : times_match_occurred_arr) {


            auto match_type_ele = ele[user_account_statistics_keys::times_match_occurred::MATCH_TYPE];

            std::optional<MatchType> match_type;

            if(match_type_ele) {
                match_type = MatchType(match_type_ele.get_int32().value);
            }

            times_match_occurred.emplace_back(
                    TimesMatchOccurred(
                            ele[user_account_statistics_keys::times_match_occurred::MATCHED_OID].get_oid().value,
                            ele[user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID].get_string().value.to_string(),
                            match_type,
                            ele[user_account_statistics_keys::times_match_occurred::TIMESTAMP].get_date()
                    )
            );
        }

        locations.clear();
        for (const auto& ele : locations_arr) {

            bsoncxx::array::view location_array_view = ele[user_account_statistics_keys::locations::LOCATION].get_array().value;
            locations.emplace_back(
                    Location(
                            location_array_view[0].get_double().value,
                            location_array_view[1].get_double().value,
                            ele[user_account_statistics_keys::locations::TIMESTAMP].get_date()
                    )
            );
        }

        phone_numbers.clear();
        for (const auto& ele : phone_numbers_arr) {
            phone_numbers.emplace_back(
                    PhoneNumber(
                            ele[user_account_statistics_keys::phone_numbers::PHONE_NUMBER].get_string().value.to_string(),
                            ele[user_account_statistics_keys::phone_numbers::TIMESTAMP].get_date()
                    )
            );
        }

        names.clear();
        for (const auto& ele : names_arr) {
            names.emplace_back(
                    Name(
                            ele[user_account_statistics_keys::names::NAME].get_string().value.to_string(),
                            ele[user_account_statistics_keys::names::TIMESTAMP].get_date()
                    )
            );
        }

        bios.clear();
        for (const auto& ele : bios_arr) {
            bios.emplace_back(
                    Bio(
                            ele[user_account_statistics_keys::bios::BIOS].get_string().value.to_string(),
                            ele[user_account_statistics_keys::bios::TIMESTAMP].get_date()
                    )
            );
        }

        cities.clear();
        for (const auto& ele : cities_arr) {
            cities.emplace_back(
                    City(
                            ele[user_account_statistics_keys::cities::CITY].get_string().value.to_string(),
                            ele[user_account_statistics_keys::cities::TIMESTAMP].get_date()
                    )
            );
        }

        genders.clear();
        for (const auto& ele : genders_arr) {
            genders.emplace_back(
                    Gender(
                            ele[user_account_statistics_keys::genders::GENDER].get_string().value.to_string(),
                            ele[user_account_statistics_keys::genders::TIMESTAMP].get_date()
                    )
            );
        }

        birth_info.clear();
        for (const auto& ele : birth_info_arr) {
            birth_info.emplace_back(
                    BirthInfo(
                            ele[user_account_statistics_keys::birth_info::YEAR].get_int32().value,
                            ele[user_account_statistics_keys::birth_info::MONTH].get_int32().value,
                            ele[user_account_statistics_keys::birth_info::DAY_OF_MONTH].get_int32().value,
                            ele[user_account_statistics_keys::birth_info::TIMESTAMP].get_date()
                    )
            );
        }

        email_addresses.clear();
        for (const auto& ele : email_addresses_arr) {
            email_addresses.emplace_back(
                    EmailAddress(
                            ele[user_account_statistics_keys::email_addresses::EMAIL].get_string().value.to_string(),
                            ele[user_account_statistics_keys::email_addresses::TIMESTAMP].get_date()
                    )
            );
        }

        categories.clear();
        for (const auto& ele : categories_arr) {
            bsoncxx::array::view categories_doc = ele[user_account_statistics_keys::categories::ACTIVITIES_AND_CATEGORIES].get_array().value;
            std::vector<TestCategory> test_categories;

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

                test_categories.emplace_back(
                        TestCategory(
                                AccountCategoryType(
                                        category_doc[user_account_keys::categories::TYPE].get_int32().value),
                                category_doc[user_account_keys::categories::INDEX_VALUE].get_int32().value,
                                timeframes
                        )
                );
            }

            categories.emplace_back(
                    StoredCategories(
                            test_categories,
                            ele[user_account_statistics_keys::categories::TIMESTAMP].get_date()
                    )
            );
        }

        age_ranges.clear();
        for (const auto& ele : age_ranges_arr) {
            bsoncxx::array::view age_range_array_view = ele[user_account_statistics_keys::age_ranges::AGE_RANGE].get_array().value;
            age_ranges.emplace_back(
                    AgeRange(
                            age_range_array_view[0].get_int32().value,
                            age_range_array_view[1].get_int32().value,
                            ele[user_account_statistics_keys::locations::TIMESTAMP].get_date()
                    )
            );
        }

        gender_ranges.clear();
        for (const auto& ele : gender_ranges_arr) {

            bsoncxx::array::view gender_ranges_array_view = ele[user_account_statistics_keys::gender_ranges::GENDER_RANGE].get_array().value;
            std::vector<std::string> gender_range;


            for (const auto& gender : gender_ranges_array_view) {

                gender_range.emplace_back(gender.get_string().value.to_string());
            }

            gender_ranges.emplace_back(
                    GenderRange(
                            gender_range,
                            ele[user_account_statistics_keys::gender_ranges::TIMESTAMP].get_date()
                    )
            );
        }

        max_distances.clear();
        for (const auto& ele : max_distances_arr) {
            max_distances.emplace_back(
                    MaxDistance(
                            ele[user_account_statistics_keys::max_distances::MAX_DISTANCE].get_int32().value,
                            ele[user_account_statistics_keys::max_distances::TIMESTAMP].get_date()
                    )
            );
        }

        sms_sent_times.clear();
        for (const auto& ele : sms_sent_times_arr) {
            sms_sent_times.emplace_back(ele.get_date());
        }

        email_sent_times.clear();
        for (const auto& ele : email_sent_times_arr) {
            email_sent_times.emplace_back(
                    EmailSentTime(
                            ele[user_account_statistics_keys::email_sent_times::EMAIL_ADDRESS].get_string().value.to_string(),
                            ele[user_account_statistics_keys::email_sent_times::EMAIL_PREFIX].get_string().value.to_string(),
                            ele[user_account_statistics_keys::email_sent_times::EMAIL_SUBJECT].get_string().value.to_string(),
                            ele[user_account_statistics_keys::email_sent_times::EMAIL_CONTENT].get_string().value.to_string(),
                            ele[user_account_statistics_keys::email_sent_times::TIMESTAMP].get_date()
                    )
            );
        }

        emails_verified.clear();
        for (const auto& ele : emails_verified_arr) {
            emails_verified.emplace_back(
                    EmailVerified(
                            ele[user_account_statistics_keys::emails_verified::EMAILS_VERIFIED_EMAIL].get_string().value.to_string(),
                            ele[user_account_statistics_keys::emails_verified::TIMESTAMP].get_date()
                    )
            );
        }

        account_recovery_times.clear();
        for (const auto& ele : account_recovery_times_arr) {
            account_recovery_times.emplace_back(
                    AccountRecoveryTime(
                            ele[user_account_statistics_keys::account_recovery_times::PREVIOUS_PHONE_NUMBER].get_string().value.to_string(),
                            ele[user_account_statistics_keys::account_recovery_times::NEW_PHONE_NUMBER].get_string().value.to_string(),
                            ele[user_account_statistics_keys::account_recovery_times::TIMESTAMP].get_date()
                    )
            );
        }

        account_logged_out_times.clear();
        for (const auto& ele : account_logged_out_times_arr) {
            account_logged_out_times.emplace_back(ele.get_date());
        }

        account_search_by_options.clear();
        for (const auto& ele : account_search_by_options_arr) {
            account_search_by_options.emplace_back(
                    SearchByOption(
                            ele[user_account_statistics_keys::search_by_options::NEW_SEARCH_BY_OPTIONS].get_int32().value,
                            ele[user_account_statistics_keys::search_by_options::TIMESTAMP].get_date()
                    )
            );
        }

        opted_in_to_promotional_email.clear();
        for (const auto& ele : opted_in_to_promotional_email_arr) {
            opted_in_to_promotional_email.emplace_back(
                    OptedInToPromotionalEmail(
                            ele[user_account_statistics_keys::opted_in_to_promotional_email::NEW_OPTED_IN_TO_PROMOTIONAL_EMAIL].get_bool().value,
                            ele[user_account_statistics_keys::opted_in_to_promotional_email::TIMESTAMP].get_date()
                    )
            );
        }

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool UserAccountStatisticsDoc::operator==(const UserAccountStatisticsDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.login_times.size() == login_times.size()) {
        for (int i = 0; i < (int)login_times.size(); i++) {
            checkForEquality(
                    login_times[i].installation_id,
                    other.login_times[i].installation_id,
                    "LOGIN_TIMES.installation_id",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    login_times[i].device_name,
                    other.login_times[i].device_name,
                    "LOGIN_TIMES.device_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    login_times[i].api_number,
                    other.login_times[i].api_number,
                    "LOGIN_TIMES.api_number",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    login_times[i].lets_go_version,
                    other.login_times[i].lets_go_version,
                    "LOGIN_TIMES.lets_go_version",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    login_times[i].timestamp,
                    other.login_times[i].timestamp,
                    "LOGIN_TIMES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                login_times.size(),
                other.login_times.size(),
                "LOGIN_TIMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.times_match_occurred.size() == times_match_occurred.size()) {
        for (size_t i = 0; i < times_match_occurred.size(); i++) {

            checkForEquality(
                    times_match_occurred[i].matched_oid.to_string(),
                    other.times_match_occurred[i].matched_oid.to_string(),
                    "TIMES_MATCH_OCCURRED.matched_oid",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    times_match_occurred[i].chat_room_id,
                    other.times_match_occurred[i].chat_room_id,
                    "TIMES_MATCH_OCCURRED.chat_room_id",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if(times_match_occurred[i].match_type && other.times_match_occurred[i].match_type) {
                checkForEquality(
                        *times_match_occurred[i].match_type,
                        *other.times_match_occurred[i].match_type,
                        "TIMES_MATCH_OCCURRED.match_type",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            } else {
                checkForEquality(
                        times_match_occurred[i].match_type.operator bool(),
                        other.times_match_occurred[i].match_type.operator bool(),
                        "TIMES_MATCH_OCCURRED.match_type.exists",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }

            checkForEquality(
                    times_match_occurred[i].timestamp,
                    other.times_match_occurred[i].timestamp,
                    "TIMES_MATCH_OCCURRED.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                times_match_occurred.size(),
                other.times_match_occurred.size(),
                "TIMES_MATCH_OCCURRED.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.locations.size() == locations.size()) {
        for (size_t i = 0; i < locations.size(); i++) {

            checkForEquality(
                    locations[i].location[0],
                    other.locations[i].location[0],
                    "LOCATIONS.location[0]",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    locations[i].location[1],
                    other.locations[i].location[1],
                    "LOCATIONS.location[1]",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    locations[i].timestamp,
                    other.locations[i].timestamp,
                    "LOCATIONS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                locations.size(),
                other.locations.size(),
                "LOCATIONS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.phone_numbers.size() == phone_numbers.size()) {
        for (size_t i = 0; i < phone_numbers.size(); i++) {

            checkForEquality(
                    phone_numbers[i].phone_number,
                    other.phone_numbers[i].phone_number,
                    "PHONE_NUMBERS.phone_number",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    phone_numbers[i].timestamp,
                    other.phone_numbers[i].timestamp,
                    "PHONE_NUMBERS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                phone_numbers.size(),
                other.phone_numbers.size(),
                "PHONE_NUMBERS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.names.size() == names.size()) {
        for (size_t i = 0; i < names.size(); i++) {

            checkForEquality(
                    names[i].name,
                    other.names[i].name,
                    "NAMES.name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    names[i].timestamp,
                    other.names[i].timestamp,
                    "NAMES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                names.size(),
                other.names.size(),
                "NAMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.bios.size() == bios.size()) {
        for (size_t i = 0; i < bios.size(); i++) {

            checkForEquality(
                    bios[i].bio,
                    other.bios[i].bio,
                    "BIOS.bio",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    bios[i].timestamp,
                    other.bios[i].timestamp,
                    "BIOS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {

        checkForEquality(
                bios.size(),
                other.bios.size(),
                "BIOS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.cities.size() == cities.size()) {
        for (size_t i = 0; i < cities.size(); i++) {
            checkForEquality(
                    cities[i].city,
                    other.cities[i].city,
                    "CITIES.city",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    cities[i].timestamp,
                    other.cities[i].timestamp,
                    "CITIES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                cities.size(),
                other.cities.size(),
                "CITIES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.genders.size() == genders.size()) {
        for (size_t i = 0; i < genders.size(); i++) {

            checkForEquality(
                    genders[i].gender,
                    other.genders[i].gender,
                    "GENDERS.gender",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    genders[i].timestamp,
                    other.genders[i].timestamp,
                    "GENDERS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                genders.size(),
                other.genders.size(),
                "GENDERS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.birth_info.size() == birth_info.size()) {
        for (size_t i = 0; i < birth_info.size(); i++) {

            checkForEquality(
                    birth_info[i].year,
                    other.birth_info[i].year,
                    "BIRTH_INFO.year",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    birth_info[i].month,
                    other.birth_info[i].month,
                    "BIRTH_INFO.month",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    birth_info[i].day_of_month,
                    other.birth_info[i].day_of_month,
                    "BIRTH_INFO.day_of_month",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    birth_info[i].timestamp,
                    other.birth_info[i].timestamp,
                    "BIRTH_INFO.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                birth_info.size(),
                other.birth_info.size(),
                "BIRTH_INFO.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.email_addresses.size() == email_addresses.size()) {
        for (size_t i = 0; i < email_addresses.size(); i++) {
            checkForEquality(
                    email_addresses[i].email,
                    other.email_addresses[i].email,
                    "EMAIL_ADDRESSES.email",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    email_addresses[i].timestamp,
                    other.email_addresses[i].timestamp,
                    "EMAIL_ADDRESSES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                email_addresses.size(),
                other.email_addresses.size(),
                "EMAIL_ADDRESSES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.categories.size() == categories.size()) {
        for (size_t i = 0; i < categories.size(); i++) {
            checkForEquality(
                    categories[i].timestamp,
                    other.categories[i].timestamp,
                    "CATEGORIES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if (categories[i].categories.size() != other.categories[i].categories.size()) {
                for (size_t j = 0; j < categories[i].categories.size(); j++) {

                    checkForEquality(
                            categories[i].categories[j].type,
                            other.categories[i].categories[j].type,
                            "CATEGORIES.categories.type",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            categories[i].categories[j].index_value,
                            other.categories[i].categories[j].index_value,
                            "CATEGORIES.categories.index_value",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    if (other.categories[i].categories[j].time_frames.size() ==
                        categories[i].categories[j].time_frames.size()) {
                        for (size_t k = 0; k < categories[i].categories[j].time_frames.size(); k++) {
                            checkForEquality(
                                    categories[i].categories[j].time_frames[k].startStopValue,
                                    other.categories[i].categories[j].time_frames[k].startStopValue,
                                    "CATEGORIES.categories.timeframes.startStopValue",
                                    OBJECT_CLASS_NAME,
                                    return_value
                            );

                            checkForEquality(
                                    categories[i].categories[j].time_frames[k].time,
                                    other.categories[i].categories[j].time_frames[k].time,
                                    "CATEGORIES.categories.timeframes.time",
                                    OBJECT_CLASS_NAME,
                                    return_value
                            );
                        }
                    } else {
                        checkForEquality(
                                categories[i].categories[j].time_frames.size(),
                                other.categories[i].categories[j].time_frames.size(),
                                "CATEGORIES.categories.timeframes.size()",
                                OBJECT_CLASS_NAME,
                                return_value
                        );
                    }

                }
            } else {
                checkForEquality(
                        categories[i].categories.size(),
                        other.categories[i].categories.size(),
                        "CATEGORIES.categories.size()",
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

    if (other.age_ranges.size() == age_ranges.size()) {
        for (size_t i = 0; i < age_ranges.size(); i++) {

            checkForEquality(
                    age_ranges[i].age_range[0],
                    other.age_ranges[i].age_range[0],
                    "AGE_RANGES.age_range[0]",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    age_ranges[i].age_range[1],
                    other.age_ranges[i].age_range[1],
                    "AGE_RANGES.age_range[1]",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    age_ranges[i].timestamp,
                    other.age_ranges[i].timestamp,
                    "AGE_RANGES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                age_ranges.size(),
                other.age_ranges.size(),
                "AGE_RANGES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.gender_ranges.size() == gender_ranges.size()) {
        for (size_t i = 0; i < gender_ranges.size(); i++) {
            checkForEquality(
                    gender_ranges[i].timestamp,
                    other.gender_ranges[i].timestamp,
                    "GENDER_RANGES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            if (other.gender_ranges[i].gender_range.size() == gender_ranges[i].gender_range.size()) {
                for (size_t j = 0; j < gender_ranges[i].gender_range.size(); j++) {
                    checkForEquality(
                            gender_ranges[i].gender_range[j],
                            other.gender_ranges[i].gender_range[j],
                            "GENDER_RANGES.gender_range",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                }
            } else {
                checkForEquality(
                        gender_ranges[i].gender_range.size(),
                        other.gender_ranges[i].gender_range.size(),
                        "GENDER_RANGES.gender_range.size",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }
        }
    } else {
        checkForEquality(
                gender_ranges.size(),
                other.gender_ranges.size(),
                "GENDER_RANGES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.max_distances.size() == max_distances.size()) {
        for (size_t i = 0; i < max_distances.size(); i++) {
            checkForEquality(
                    max_distances[i].timestamp,
                    other.max_distances[i].timestamp,
                    "MAX_DISTANCES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    max_distances[i].max_distance,
                    other.max_distances[i].max_distance,
                    "MAX_DISTANCES.max_distance",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                max_distances.size(),
                other.max_distances.size(),
                "MAX_DISTANCES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.sms_sent_times.size() == sms_sent_times.size()) {
        for (size_t i = 0; i < sms_sent_times.size(); i++) {
            checkForEquality(
                    sms_sent_times[i].value.count(),
                    other.sms_sent_times[i].value.count(),
                    "SMS_SENT_TIMES",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                sms_sent_times.size(),
                other.sms_sent_times.size(),
                "SMS_SENT_TIMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.email_sent_times.size() == email_sent_times.size()) {
        for (size_t i = 0; i < email_sent_times.size(); i++) {
            checkForEquality(
                    email_sent_times[i].email_address,
                    other.email_sent_times[i].email_address,
                    "EMAIL_SENT_TIMES.email_address",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    email_sent_times[i].email_prefix,
                    other.email_sent_times[i].email_prefix,
                    "EMAIL_SENT_TIMES.email_prefix",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    email_sent_times[i].email_subject,
                    other.email_sent_times[i].email_subject,
                    "EMAIL_SENT_TIMES.email_subject",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    email_sent_times[i].email_content,
                    other.email_sent_times[i].email_content,
                    "EMAIL_SENT_TIMES.email_content",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    email_sent_times[i].timestamp,
                    other.email_sent_times[i].timestamp,
                    "EMAIL_SENT_TIMES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                email_sent_times.size(),
                other.email_sent_times.size(),
                "EMAIL_SENT_TIMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.emails_verified.size() == emails_verified.size()) {
        for (size_t i = 0; i < emails_verified.size(); i++) {

            checkForEquality(
                    emails_verified[i].emails_verified_email,
                    other.emails_verified[i].emails_verified_email,
                    "EMAILS_VERIFIED.emails_verified_email",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    emails_verified[i].timestamp,
                    other.emails_verified[i].timestamp,
                    "EMAILS_VERIFIED.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                emails_verified.size(),
                other.emails_verified.size(),
                "EMAILS_VERIFIED.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.account_recovery_times.size() == account_recovery_times.size()) {
        for (size_t i = 0; i < account_recovery_times.size(); i++) {

            checkForEquality(
                    account_recovery_times[i].previous_phone_number,
                    other.account_recovery_times[i].previous_phone_number,
                    "ACCOUNT_RECOVERY_TIMES.previous_phone_number",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    account_recovery_times[i].new_phone_number,
                    other.account_recovery_times[i].new_phone_number,
                    "ACCOUNT_RECOVERY_TIMES.new_phone_number",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    account_recovery_times[i].timestamp,
                    other.account_recovery_times[i].timestamp,
                    "ACCOUNT_RECOVERY_TIMES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                account_recovery_times.size(),
                other.account_recovery_times.size(),
                "ACCOUNT_RECOVERY_TIMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.account_logged_out_times.size() == account_logged_out_times.size()) {
        for (size_t i = 0; i < account_logged_out_times.size(); i++) {
            checkForEquality(
                    account_logged_out_times[i].value.count(),
                    other.account_logged_out_times[i].value.count(),
                    "ACCOUNT_LOGGED_OUT_TIMES.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                account_logged_out_times.size(),
                other.account_logged_out_times.size(),
                "ACCOUNT_LOGGED_OUT_TIMES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.account_search_by_options.size() == account_search_by_options.size()) {
        for (size_t i = 0; i < account_search_by_options.size(); i++) {
            checkForEquality(
                    account_search_by_options[i].new_search_by_options,
                    other.account_search_by_options[i].new_search_by_options,
                    "ACCOUNT_SEARCH_BY_OPTIONS.new_search_by_options",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    account_search_by_options[i].timestamp,
                    other.account_search_by_options[i].timestamp,
                    "ACCOUNT_SEARCH_BY_OPTIONS.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                account_search_by_options.size(),
                other.account_search_by_options.size(),
                "ACCOUNT_SEARCH_BY_OPTIONS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.opted_in_to_promotional_email.size() == opted_in_to_promotional_email.size()) {
        for (size_t i = 0; i < opted_in_to_promotional_email.size(); i++) {
            checkForEquality(
                    opted_in_to_promotional_email[i].new_opted_in_to_promotional_email,
                    other.opted_in_to_promotional_email[i].new_opted_in_to_promotional_email,
                    "OPTED_IN_TO_PROMOTIONAL_EMAIL.new_opted_in_to_promotional_email",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    opted_in_to_promotional_email[i].timestamp,
                    other.opted_in_to_promotional_email[i].timestamp,
                    "OPTED_IN_TO_PROMOTIONAL_EMAIL.timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                opted_in_to_promotional_email.size(),
                other.opted_in_to_promotional_email.size(),
                "OPTED_IN_TO_PROMOTIONAL_EMAIL.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const UserAccountStatisticsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
