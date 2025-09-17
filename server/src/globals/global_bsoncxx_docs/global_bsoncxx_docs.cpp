//
// Created by jeremiah on 3/29/21.
//

#include "global_bsoncxx_docs.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** DO NOT AUTO FORMAT FILE **/

bsoncxx::document::value buildDocumentToCalculateAge(const std::chrono::milliseconds& currentTimestamp) {

    const time_t timeObject = currentTimestamp.count() / 1000;
    tm dateTimeStruct{};
    gmtime_r(&timeObject, &dateTimeStruct);

    //calculate and update age
    return document{}
            << "$let" << open_document
                << "vars" << open_document
                    << "current_age" << open_document

                        //calculate age
                        << "$subtract" << open_array
                            << open_document
                                << "$subtract" << open_array
                                    << bsoncxx::types::b_int32{dateTimeStruct.tm_year + 1900} << '$' + user_account_keys::BIRTH_YEAR
                                << close_array
                            << close_document
                            << open_document
                                << "$cond" << open_array
                                    << open_document
                                        << "$gt" << open_array
                                            << bsoncxx::types::b_int32{0}
                                            << open_document
                                                << "$subtract" << open_array
                                                    << bsoncxx::types::b_int32{dateTimeStruct.tm_yday} << '$' + user_account_keys::BIRTH_DAY_OF_YEAR
                                                << close_array
                                            << close_document
                                        << close_array
                                    << close_document
                                    << bsoncxx::types::b_int32{1}
                                    << bsoncxx::types::b_int32{0}
                                << close_array
                            << close_document
                        << close_array

                    << close_document
                << close_document
                << "in" << open_document

                    //make sure age never goes over HIGHEST_ALLOWED_AGE
                    << "$cond" << open_document
                        << "if" << open_document
                            << "$lt" << open_array
                                << "$$current_age" << server_parameter_restrictions::HIGHEST_ALLOWED_AGE
                            << close_array
                        << close_document
                        << "then" << "$$current_age"
                        << "else" << server_parameter_restrictions::HIGHEST_ALLOWED_AGE
                    << close_document

                << close_document
            << close_document
            << finalize;

}

bsoncxx::document::value getResetSmsIfOffCoolDown(
        const bsoncxx::types::b_date& currentDateMongo,
        const std::string& coolDownOnSmsKey,
        const std::string& timeSmsCanBeSentAgainKey
        ) {

    //This is required because mongoDB $add cannot take 2 date objects.
    bsoncxx::types::b_int64 current_date_in_ms{currentDateMongo.value.count()};

    return document{}
        << "$cond" << open_document

            //if sms is no longer on cool down
            << "if" << open_document
                << "$lte" << open_array
                    << "$" + timeSmsCanBeSentAgainKey << currentDateMongo
                << close_array
            << close_document

            //then set up a new cool down (assume the login function will send it)
            // and return -1 meaning no cool down remaining
            << "then" << open_document
                << coolDownOnSmsKey << -1
                << timeSmsCanBeSentAgainKey << open_document
                    << "$add" << open_array
                        << current_date_in_ms << bsoncxx::types::b_date{general_values::TIME_BETWEEN_SENDING_SMS}
                    << close_array
                << close_document
            << close_document

            //if sms is still on cool down, save the cool down to the variable
            << "else" << open_document
                << coolDownOnSmsKey << open_document

                    //convert cool down to 'seconds' as an int_32
                    << "$toInt" << open_document
                        << "$divide" << open_array
                            << open_document
                                << "$subtract" << open_array
                                    << open_document
                                        << "$toLong" << "$" + timeSmsCanBeSentAgainKey
                                    << close_document
                                    << current_date_in_ms
                                << close_array
                            << close_document
                            << 1000
                        << close_array
                    << close_document

                << close_document
                << timeSmsCanBeSentAgainKey << "$" + timeSmsCanBeSentAgainKey
            << close_document

        << close_document
    << finalize;

}

bsoncxx::document::value getLoginFunctionDocument(
        const std::string& account_id,
        const std::string& installation_id,
        const std::chrono::milliseconds& current_timestamp
        ) {

    const bsoncxx::types::b_date current_date_mongo = bsoncxx::types::b_date{current_timestamp};

    auto aggregation_var_reference = [](const std::string& var)->std::string {
        return "$$" + var;
    };

    auto calculate_not_doc = [&aggregation_var_reference](const std::string& var)->bsoncxx::document::value {
        return document{}
                << "$not" << open_array
                    << aggregation_var_reference(var)
                << close_array
            << finalize;
    };

    const bsoncxx::document::value account_suspended_but_suspension_expired = document{}
            << "$and" << open_array

                << open_document
                    << "$eq" << open_array
                        << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_SUSPENDED
                    << close_array
                << close_document

                << open_document
                    << "$lte" << open_array
                        << "$" + user_account_keys::INACTIVE_END_TIME << current_date_mongo
                    << close_array
                << close_document

            << close_array
        << finalize;

    bsoncxx::builder::stream::document boolean_variables;

    static const std::string ACCOUNT_ACCESSIBLE = "aiA";

    boolean_variables
            << ACCOUNT_ACCESSIBLE << open_document
                << "$or" << open_array
                    << open_document
                        << "$eq" << open_array
                            << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                        << close_array
                    << close_document
                    << open_document
                        << "$eq" << open_array
                            << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                        << close_array
                    << close_document

                    //account suspended, however suspension has ended
                    << account_suspended_but_suspension_expired

                << close_array
            << close_document;

    static const std::string INSTALL_ID_EXISTS = "iiE";

    boolean_variables
            << INSTALL_ID_EXISTS << open_document
                << "$in" << open_array
                    << installation_id << "$" + user_account_keys::INSTALLATION_IDS
                << close_array
            << close_document;

    static const std::string ACCOUNT_ID_EXISTS = "aiE";

    if(!account_id.empty()) {
        //this is necessary if an account id attempts to connect to a phone number from a new
        // device (a new account id and new installation id are passed to an existing account)
        boolean_variables
                << ACCOUNT_ID_EXISTS << open_document
                    << "$in" << open_array
                        << open_document
                            << "$literal" << account_id
                        << close_document
                        << "$" + user_account_keys::ACCOUNT_ID_LIST
                    << close_array
                << close_document;
    }

    static const std::string VERIFIED_TIME_OK = "vtO";

    boolean_variables
                << VERIFIED_TIME_OK << open_document
                    << "$lt" << open_array
                        << open_document
                            << "$toLong" << open_document
                                << "$subtract" << open_array
                                    << current_date_mongo << "$" + user_account_keys::LAST_VERIFIED_TIME
                                << close_array
                            << close_document
                        << close_document
                        << general_values::TIME_BETWEEN_ACCOUNT_VERIFICATION.count()
                    << close_array
                << close_document;

    static const std::string INSTALL_ID_MATCHES = "iiM";

    boolean_variables
                << INSTALL_ID_MATCHES << open_document
                    << "$eq" << open_array
                        << "$" + user_account_keys::LOGGED_IN_INSTALLATION_ID << installation_id
                    << close_array
                << close_document;

    static const std::string LOGIN_TOKEN_TIME_OK = "lltO";

    boolean_variables
                << LOGIN_TOKEN_TIME_OK << open_document
                    << "$lt" << open_array
                        << current_date_mongo << "$" + user_account_keys::LOGGED_IN_TOKEN_EXPIRATION
                    << close_array
                << close_document;

    bsoncxx::builder::basic::array cases_builder;

    bsoncxx::builder::stream::array success_conditions_array_builder{};

    success_conditions_array_builder
            << aggregation_var_reference(ACCOUNT_ACCESSIBLE)
            << aggregation_var_reference(INSTALL_ID_EXISTS);

    if(!account_id.empty()) {
        success_conditions_array_builder
            << aggregation_var_reference(ACCOUNT_ID_EXISTS);
    }

    success_conditions_array_builder
        << aggregation_var_reference(VERIFIED_TIME_OK)
        << aggregation_var_reference(INSTALL_ID_MATCHES)
        << aggregation_var_reference(LOGIN_TOKEN_TIME_OK);

    bsoncxx::document::value suspension_expired_doc = document{}
            << "$let" << open_document
                << "vars" << open_document
                    //checks if account is suspended AND suspension is expired
                    << "suspension_expired" << open_document
                        << "$cond" << open_document
                            << "if" << account_suspended_but_suspension_expired
                            << "then" << true
                            << "else" << false
                        << close_document
                    << close_document
                << close_document
                << "in" << open_document

                    //NOTE: A user with a status of STATUS_REQUIRES_MORE_INFO can NOT be
                    // changed by an admin (it cannot go from STATUS_REQUIRES_MORE_INFO -> STATUS_SUSPENDED).
                    // This means there is no chance of a user going through
                    // STATUS_REQUIRES_MORE_INFO->STATUS_SUSPENDED->STATUS_ACTIVE and being active w/o enough info.
                    << user_account_keys::STATUS << open_document
                        << "$cond" << open_document
                            << "if" << "$$suspension_expired"
                            << "then" << UserAccountStatus::STATUS_ACTIVE
                            << "else" << "$" + user_account_keys::STATUS
                        << close_document
                    << close_document

                    << user_account_keys::INACTIVE_MESSAGE << open_document
                        << "$cond" << open_document
                            << "if" << "$$suspension_expired"
                            << "then" << ""
                            << "else" << "$" + user_account_keys::INACTIVE_MESSAGE
                        << close_document
                    << close_document

                    << user_account_keys::INACTIVE_END_TIME << open_document
                        << "$cond" << open_document
                            << "if" << "$$suspension_expired"
                            << "then" << bsoncxx::types::b_null{}
                            << "else" << "$" + user_account_keys::INACTIVE_END_TIME
                        << close_document
                    << close_document

                << close_document
            << close_document
        << finalize;

    const bsoncxx::types::b_date login_token_expiration_time = bsoncxx::types::b_date{current_timestamp + general_values::TIME_BETWEEN_TOKEN_VERIFICATION};

    //all conditions matched, refresh login token
    cases_builder.append(document{}
            << "case" << open_document
                << "$and" << success_conditions_array_builder
            << close_document
            << "then" << open_document

                << "$mergeObjects" << open_array
                    << open_document
                        << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << login_token_expiration_time
                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << LoginFunctionResultValuesEnum::LOGIN_USER_LOGGED_IN
                    << close_document
                    << suspension_expired_doc
                << close_array

            << close_document
        << finalize
    );

    enum AdditionalInfoEnum {
        ADD_ONLY_VALUE,
        GENERATE_NEW_LOGIN_TOKEN,
        UPDATE_LAST_TIME_SMS_SENT
    };

    auto append_failure_case =
            [&cases_builder,
            &login_token_expiration_time,
            &installation_id,
            &current_date_mongo,
            &suspension_expired_doc]
            (const bsoncxx::document::view& caseDoc,
            const LoginFunctionResultValuesEnum& value,
            const AdditionalInfoEnum& directions)
    {

        if(directions == AdditionalInfoEnum::ADD_ONLY_VALUE) {
            cases_builder.append(
                document{}
                    << "case" << caseDoc
                    << "then" << open_document
                        << "$mergeObjects" << open_array
                            << open_document
                                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << value
                            << close_document
                            << suspension_expired_doc
                        << close_array
                    << close_document
                << finalize
            );
        } else if(directions == AdditionalInfoEnum::GENERATE_NEW_LOGIN_TOKEN) {

            cases_builder.append(
                document{}
                    << "case" << caseDoc
                    << "then" << open_document
                        << "$mergeObjects" << open_array

                            << open_document
                                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << value
                                << user_account_keys::LOGGED_IN_TOKEN << bsoncxx::oid{}.to_string()
                                << user_account_keys::LOGGED_IN_INSTALLATION_ID << installation_id
                                << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << login_token_expiration_time
                            << close_document
                            << suspension_expired_doc

                        << close_array
                    << close_document
                    << finalize
                );
        } else { //AdditionalInfoEnum::UPDATE_LAST_TIME_SMS_SENT

            auto sms_on_cool_down_doc = getResetSmsIfOffCoolDown(
                    current_date_mongo,
                    user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE,
                    user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN
            );

            cases_builder.append(
                document{}
                    << "case" << caseDoc
                    << "then" << open_document

                        << "$mergeObjects" << open_array

                            //sets
                            //LOGGED_IN_RETURN_MESSAGE
                            << open_document
                                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << value
                            << close_document

                            //sets
                            //COOL_DOWN_ON_SMS_RETURN_MESSAGE
                            //TIME_SMS_CAN_BE_SENT_AGAIN
                            << sms_on_cool_down_doc.view()

                            //sets
                            //STATUS
                            //INACTIVE_MESSAGE
                            //INACTIVE_END_TIME
                            << suspension_expired_doc
                        << close_array
                    << close_document
                << finalize
            );
        }
    };

    /***********************************************/
    /** NOTE: Order of switch cases matters here! **/
    /***********************************************/

    //account banned
    append_failure_case(
            document{}
                        << "$eq" << open_array
                            << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_BANNED
                        << close_array
                    << finalize,
            LoginFunctionResultValuesEnum::LOGIN_BANNED,
            AdditionalInfoEnum::ADD_ONLY_VALUE
    );

    //account suspended and suspension time not up yet
    append_failure_case(
        document{}
            << "$and" << open_array

                << open_document
                    << "$eq" << open_array
                        << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_SUSPENDED
                    << close_array
                << close_document

                << open_document
                    << "$gt" << open_array
                        << "$" + user_account_keys::INACTIVE_END_TIME << current_date_mongo
                    << close_array
                << close_document

            << close_array
        << finalize,
        LoginFunctionResultValuesEnum::LOGIN_SUSPENDED,
        AdditionalInfoEnum::ADD_ONLY_VALUE
    );

    //installation ids does not exist inside INSTALLATION_IDS
    append_failure_case(
            calculate_not_doc(INSTALL_ID_EXISTS),
            LoginFunctionResultValuesEnum::LOGIN_INSTALLATION_DOES_NOT_EXIST,
            AdditionalInfoEnum::UPDATE_LAST_TIME_SMS_SENT
    );

    if(!account_id.empty()) {
        //account ID does not exist inside ACCOUNT_ID_LIST
        append_failure_case(
                calculate_not_doc(ACCOUNT_ID_EXISTS),
                LoginFunctionResultValuesEnum::LOGIN_ACCOUNT_ID_DOES_NOT_EXIST,
                AdditionalInfoEnum::UPDATE_LAST_TIME_SMS_SENT
        );
    }

    //verified account time has expired
    append_failure_case(
            calculate_not_doc(VERIFIED_TIME_OK),
            LoginFunctionResultValuesEnum::LOGIN_VERIFICATION_TIME_EXPIRED,
            AdditionalInfoEnum::UPDATE_LAST_TIME_SMS_SENT
    );

    /** USER OK TO LOG IN **/

    //installation id does not match stored installation id
    append_failure_case(
            calculate_not_doc(INSTALL_ID_MATCHES),
            LoginFunctionResultValuesEnum::LOGIN_INSTALLATION_ID_DOES_NOT_MATCH,
            AdditionalInfoEnum::GENERATE_NEW_LOGIN_TOKEN
    );

    //expired login token
    append_failure_case(
            calculate_not_doc(LOGIN_TOKEN_TIME_OK),
            LoginFunctionResultValuesEnum::LOGIN_LOGIN_TOKEN_EXPIRED,
            AdditionalInfoEnum::GENERATE_NEW_LOGIN_TOKEN
    );

    return document{}
        << "newRoot" << open_document
            << "$let" << open_document
                << "vars" << open_document

                    //generate document to merge below; $$mergeDoc is expected to be a document type
                    // with a field for LOGGED_IN_RETURN_MESSAGE
                    << "mergeDoc" << open_document

                        << "$let" << open_document
                            << "vars" << boolean_variables.view()
                            << "in" << open_document
                                << "$switch" << open_document
                                    << "branches" << cases_builder

                                    //NOTE: Reaching here means that there was an error with logic.
                                    << "default" << open_document
                                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << LoginFunctionResultValuesEnum::LOGIN_ERROR_UNKNOWN_VALUE
                                    << close_document

                                << close_document
                            << close_document
                        << close_document

                    << close_document

                << close_document
                << "in" << open_document

                    << "$mergeObjects" << open_array
                        << "$$ROOT"
                        << "$$mergeDoc"
                    << close_array

                << close_document
            << close_document
        << close_document
    << finalize;

}

const std::string ADD_STRING = "$add"; // NOLINT
const std::string SUBTRACT_STRING = "$subtract"; // NOLINT

const std::string OR_STRING = "$or"; // NOLINT
const std::string AND_STRING = "$and"; // NOLINT

const std::string GT_STRING = "$gt"; // NOLINT
const std::string LT_STRING = "$lt"; // NOLINT
const std::string LTE_STRING = "$lte"; // NOLINT
const std::string EQ_STRING = "$eq"; // NOLINT

template<typename T, typename U>
bsoncxx::document::value buildOperationDoc(const std::string& comparison, const T& lhs, const U& rhs) {
    return document{}
            << comparison << open_array
                << lhs << rhs
            << close_array
        << finalize;
}

template<typename T, typename U>
bsoncxx::document::value
buildCondDoc(const bsoncxx::document::view& comparison, const T& thenValue, const U& elseValue) {
    return document{}
            << "$cond" << open_document
                << "if" << comparison
                << "then" << thenValue
                << "else" << elseValue
            << close_document
        << finalize;
}

bsoncxx::document::value
buildSwitch(const std::string& USER_AGE_FROM_DOC, const std::vector<bsoncxx::document::value>& cases) {

    bsoncxx::builder::basic::array branches;

    for (const auto& singleCase : cases) {
        branches.append(singleCase.view());
    }

    return document{}
            << "$switch" << open_document
                << "branches" << branches
                << "default" << USER_AGE_FROM_DOC
                << close_document
            << finalize;
}

bsoncxx::document::value buildCaseDoc(
        const std::string& andOrString,
        const bsoncxx::document::view& topLevelComparisonFirst,
        const bsoncxx::document::view& topLevelComparisonSecond,
        const bsoncxx::document::view& conditionStatement
) {

    return document{}
            << "case" << open_document
                << andOrString << open_array
                    << topLevelComparisonFirst
                    << topLevelComparisonSecond
                << close_array
            << close_document
            << "then" << conditionStatement
            << finalize;
}

//NOTE: The variable names come from roughly following buildAgeRangeChecker().
bsoncxx::document::value buildCompleteSwitchDocument(int min_or_max_age) {

    const std::string USER_AGE_FROM_DOC = "$" + user_account_keys::AGE;
    std::vector<bsoncxx::document::value> cases;

    const auto age_13_15_first_cond = buildOperationDoc(
            LTE_STRING,
            server_parameter_restrictions::LOWEST_ALLOWED_AGE,
            USER_AGE_FROM_DOC
    );
    const auto age_13_15_second_cond = buildOperationDoc(
            LTE_STRING,
            USER_AGE_FROM_DOC,
            15
    );

    const auto age_13_15_lt = buildOperationDoc(
            LT_STRING,
            17,
            min_or_max_age
    );
    const auto age_13_15_cond = buildCondDoc(
            age_13_15_lt.view(),
            17,
            min_or_max_age
    );

    cases.emplace_back(
            buildCaseDoc(
                    AND_STRING,
                    age_13_15_first_cond,
                    age_13_15_second_cond,
                    age_13_15_cond)
    );

    const auto age_16_17_first_cond = buildOperationDoc(EQ_STRING, 16, USER_AGE_FROM_DOC);
    const auto age_16_17_second_cond = buildOperationDoc(EQ_STRING, USER_AGE_FROM_DOC, 17);
    const auto age_16_17_age_plus_two = buildOperationDoc(ADD_STRING, USER_AGE_FROM_DOC, 2);

    const auto age_16_17_lt = buildOperationDoc(LT_STRING, age_16_17_age_plus_two.view(), min_or_max_age);
    const auto age_16_17_cond = buildCondDoc(age_16_17_lt.view(), age_16_17_age_plus_two.view(), min_or_max_age);
    cases.emplace_back(
            buildCaseDoc(
                    OR_STRING,
                    age_16_17_first_cond,
                    age_16_17_second_cond,
                    age_16_17_cond)
    );

    const auto age_18_19_first_cond = buildOperationDoc(EQ_STRING, 18, USER_AGE_FROM_DOC);
    const auto age_18_19_second_cond = buildOperationDoc(EQ_STRING, USER_AGE_FROM_DOC, 19);
    const auto age_18_19_age_subtract_two = buildOperationDoc(SUBTRACT_STRING, USER_AGE_FROM_DOC, 2);

    const auto age_18_19_gt = buildOperationDoc(GT_STRING, age_18_19_age_subtract_two.view(), min_or_max_age);
    const auto age_18_19_cond = buildCondDoc(age_18_19_gt.view(), age_18_19_age_subtract_two.view(), min_or_max_age);
    cases.emplace_back(
            buildCaseDoc(
                    OR_STRING,
                    age_18_19_first_cond,
                    age_18_19_second_cond,
                    age_18_19_cond)
    );

    const auto age_20_first_cond = buildOperationDoc(LTE_STRING, 20, USER_AGE_FROM_DOC);
    const auto age_20_second_cond = buildOperationDoc(LTE_STRING, USER_AGE_FROM_DOC, server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    const auto age_20_gt = buildOperationDoc(GT_STRING, 18, min_or_max_age);
    const auto age_20_cond = buildCondDoc(age_20_gt.view(), 18, min_or_max_age);
    cases.emplace_back(
            buildCaseDoc(
                    OR_STRING,
                    age_20_first_cond,
                    age_20_second_cond,
                    age_20_cond)
    );

    return buildSwitch(USER_AGE_FROM_DOC, cases);
}

//NOTE: See other buildAgeRangeChecker() function below.
bsoncxx::document::value buildAgeRangeChecker(int min_age, int max_age) {

    //NOTE: This logic may be easier to follow in the C++ function (same function name).
    return document{}
            << "$let" << open_document
                << "vars" << open_document
                    //buildCompleteSwitchDocument() parameters are expected to be within the range of
                    // [LOWEST_ALLOWED_AGE, HIGHEST_ALLOWED_AGE].
                    << "minAge" << buildCompleteSwitchDocument(min_age)
                    << "maxAge" << buildCompleteSwitchDocument(max_age)
                << close_document
                << "in" << open_document

                    //if (maxAge < minAge) {
                    //    maxAge = minAge;
                    //}
                    << "$cond" << open_document
                        << "if" << open_document
                            << "$lt" << open_array
                                << "$$maxAge" << "$$minAge"
                            << close_array
                        << close_document
                        << "then" << open_document
                            << user_account_keys::age_range::MIN << "$$minAge"
                            << user_account_keys::age_range::MAX << "$$minAge"
                        << close_document
                        << "else" << open_document
                            << user_account_keys::age_range::MIN << "$$minAge"
                            << user_account_keys::age_range::MAX << "$$maxAge"
                        << close_document
                    << close_document

                << close_document
            << close_document
            << finalize;

}

//NOTE: See other buildAgeRangeChecker() function above.
void buildAgeRangeChecker(int user_age, int& min_age, int& max_age) {

    //NOTE: This follows the same logic as the other function with the same name.
    if (server_parameter_restrictions::LOWEST_ALLOWED_AGE <= user_age
        && user_age <= 15) { //if age is between 13->15 the age range must be between 13->17 (13 is expected to be checked for)
        if (17 < min_age) {
            min_age = 17;
        }
        if (17 < max_age) {
            max_age = 17;
        }
    } else if (16 == user_age
               || user_age == 17) { //if age is 16 or 17 then age range must be between 13->(userAge+2) (13 is expected to be checked for)
        if (user_age + 2 < min_age) {
            min_age = user_age + 2;
        }
        if (user_age + 2 < max_age) {
            max_age = user_age + 2;
        }
    } else if (18 == user_age || user_age == 19) {  //if age is 18 or 19 then age range must be between (userAge-2)-120
        if (user_age - 2 > min_age) {
            min_age = user_age - 2;
        }
        if (user_age - 2 > max_age) {
            max_age = user_age - 2;
        }
    } else if (20 <= user_age
               && user_age <= server_parameter_restrictions::HIGHEST_ALLOWED_AGE) { //if age is 20 or above the age range must be between 18->120 (120 is checked for above)
        if (18 > min_age) {
            min_age = 18;
        }
    }

    if (max_age < min_age) {
        max_age = min_age;
    }
}

bsoncxx::builder::stream::document buildSmsVerificationFindAndUpdateOneDoc(
        const std::string& installationId,
        const std::chrono::milliseconds& currentTimestamp,
        const AccountLoginType& pendingDocAccountType,
        const std::string& accountId,
        const sms_verification::SMSVerificationRequest* request
) {

//    Sudo code for how this document works
//    if(installationId in INSTALLATION_IDS) {
//        return INSTALLATION_IDS;
//    } else if(CREATE_NEW_ACCOUNT) {
//        return [];
//    } else if(birthdayMatches) {
//        INSTALLATION_IDS.add(installationId);
//        INSTALLATION_IDS.max_size() == MAX_NUMBER_OF_INSTALLATION_IDS_STORED;
//        return INSTALLATION_IDS;
//    } else { //means birthday did not match
//        return INSTALLATION_IDS; //will not have installation Id
//    }

    bsoncxx::builder::basic::array update_install_ids_cases;

    //if installation Id already exists inside account, verification is finished; no need to change anything
    update_install_ids_cases.append(
        document{}
            << "case" << open_document
                << "$in" << open_array
                    << installationId << "$" + user_account_keys::INSTALLATION_IDS
                << close_array
            << close_document
            << "then" << "$" + user_account_keys::INSTALLATION_IDS
        << finalize
    );

    //if installation Id does NOT exist inside the account, need to check what the user wants to do to add it
    // if they want to create a new account, set the array to empty to signal deleting this account and creating a new one
    update_install_ids_cases.append(
        document{}
            << "case" << (request->installation_id_added_command() == sms_verification::SMSVerificationRequest_InstallationIdAddedCommand_CREATE_NEW_ACCOUNT)
            << "then" << open_array << close_array
        << finalize
    );

    //if they do NOT want to create an account (there are other fields here, however they can be checked for an error after the query)
    // check if birthday matches, if it does then
    // 1) add installation Id to array
    // 2) trim the array if it gets too long
    update_install_ids_cases.append(
        document{}
            << "case" << open_document

                //if passed birthday matches
                << "$and" << open_array
                    << (request->installation_id_added_command() == sms_verification::SMSVerificationRequest_InstallationIdAddedCommand_UPDATE_ACCOUNT)
                    << open_document
                        << "$eq" << open_array
                            << request->installation_id_birth_year() << "$" + user_account_keys::BIRTH_YEAR
                        << close_array
                    << close_document
                    << open_document
                        << "$eq" << open_array
                            << request->installation_id_birth_month() << "$" + user_account_keys::BIRTH_MONTH
                        << close_array
                    << close_document
                    << open_document
                        << "$eq" << open_array
                            << request->installation_id_birth_day_of_month() << "$" + user_account_keys::BIRTH_DAY_OF_MONTH
                        << close_array
                    << close_document
                << close_array

            << close_document
            << "then" << open_document

                << "$let" << open_document
                    << "vars" << open_document

                        //save the variable $$newArray to the concatenated array
                        << "newArray" << open_document
                            << "$concatArrays" << open_array
                                //Want installation_id to be put at the FRONT of the array because the ones at the back will be
                                // trimmed if too many exist first.
                                << open_array
                                    << installationId
                                << close_array
                                << "$" + user_account_keys::INSTALLATION_IDS
                            << close_array
                        << close_document

                    << close_document
                    << "in" << open_document

                        //if $$newArray is too long, remove some end elements
                        << "$slice" << open_array
                            << "$$newArray" << general_values::MAX_NUMBER_OF_INSTALLATION_IDS_STORED
                        << close_array

                    << close_document
                << close_document

            << close_document
        << finalize
    );

    bsoncxx::builder::stream::document update_user_pipeline_document;

    update_user_pipeline_document
        << user_account_keys::LAST_VERIFIED_TIME << bsoncxx::types::b_date{currentTimestamp}
        << user_account_keys::INSTALLATION_IDS << open_document

            << "$switch" << open_document
                << "branches" << update_install_ids_cases.view()

                //Getting to default could mean
                // 1) the installation Id did not exist
                // 2) the user did not pass CREATE NEW ACCOUNT
                // 3) the birthday did not match so no update was performed
                << "default" << "$" + user_account_keys::INSTALLATION_IDS
            << close_document

        << close_document;

    if (pendingDocAccountType == AccountLoginType::GOOGLE_ACCOUNT
        || pendingDocAccountType == AccountLoginType::FACEBOOK_ACCOUNT
    ) { //if account has account Id

        //NOTE: ACCOUNT_ID_LIST is an indexed field, so it should be updated
        // it as little as possible.
        update_user_pipeline_document
            << user_account_keys::ACCOUNT_ID_LIST << open_document
                << "$cond" << open_document

                    // if account Id exists inside array
                    << "if" << open_document
                        << "$in" << open_array
                            << accountId << "$" + user_account_keys::ACCOUNT_ID_LIST
                        << close_array
                    << close_document

                    //do nothing to the list
                    << "then" << "$" + user_account_keys::ACCOUNT_ID_LIST

                    //add the account id onto the array
                    << "else" << open_document
                        << "$concatArrays" << open_array
                            << "$" + user_account_keys::ACCOUNT_ID_LIST
                            << open_array
                                << accountId
                            << close_array
                        << close_array
                    << close_document

                << close_document
            << close_document;

    }

    return update_user_pipeline_document;

}

bsoncxx::array::value buildDefaultCategoriesArray() {

    bsoncxx::builder::basic::array arrayBuilder{};

    arrayBuilder.append(document{}
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0 //upsert first activity
                << user_account_keys::categories::TIMEFRAMES << open_array
                << close_array //upsert empty timeframes array for 'anytime'
            << finalize);

    arrayBuilder.append(document{}
            << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
            << user_account_keys::categories::INDEX_VALUE << 0 //upsert first category
            << user_account_keys::categories::TIMEFRAMES << open_array
            << close_array //upsert empty timeframes array for 'anytime'
            << finalize);

    return arrayBuilder.extract();
}

bsoncxx::document::value getCategoriesDocWithChecks(const std::chrono::milliseconds& earliestStartTime) {

    bsoncxx::array::value defaultCategoryArray = buildDefaultCategoriesArray();

    return document{}
        << "$cond" << open_document

            //if: categories document is empty
            << "if" << open_document
                << "$eq" << open_array
                    << 0
                    << open_document
                        << "$size" << "$" + user_account_keys::CATEGORIES
                    << close_document
                << close_array
            << close_document

            //then: add a category to the array
            << "then" << defaultCategoryArray

            //else: move on with checking for outdated timeframes
            << "else" << open_document
                << "$map" << open_document
                    << "input" << "$" + user_account_keys::CATEGORIES
                    << "as" << "category"
                    << "in" << open_document
                        << "$mergeObjects" << open_array
                            << "$$category"
                            << open_document
                                << user_account_keys::categories::TIMEFRAMES << open_document
                                    << "$filter" << open_document
                                        << "input" << "$$category." + user_account_keys::categories::TIMEFRAMES
                                        << "as" << "timestamp"
                                        << "cond" << open_document
                                            << "$gt" << open_array
                                                << "$$timestamp." + user_account_keys::categories::timeframes::TIME
                                                << bsoncxx::types::b_int64{earliestStartTime.count()}
                                            << close_array
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << close_array
                    << close_document
                << close_document
            << close_document

        << close_document

    << finalize;

}

bsoncxx::document::value buildSetCategoryOrActivityDoc(
        const bsoncxx::document::view& activity_or_category_doc_view,
        const std::string& ARRAY_FIELD_KEY_TO_UPDATE,
        const bsoncxx::array::view& equality_array
) {

    //This will upsert the passed document activity_or_category_doc_view to the array field (matching based on name).
    return document{}
        << ARRAY_FIELD_KEY_TO_UPDATE << open_document

            << "$let" << open_document

                << "vars" << open_document
                    << "reduce_result" << open_document

                        << "$reduce" << open_document

                            //appending an extra array element for arrays of size 0
                            << "input" << "$" + ARRAY_FIELD_KEY_TO_UPDATE

                            //NOTE: While this starts as a document, the array will be returned.
                            << "initialValue" << open_document
                                << "array" << open_array
                                << close_array
                                << "value_found" << false
                            << close_document

                            << "in" << open_document
                                << "$cond" << open_document

                                    //if: value_found == false, name == stored_name &(if relevant) category_index matches
                                    << "if" << open_document
                                        << "$and" << equality_array
                                    << close_document

                                    //then: set value_found to false and add on the generated document
                                    // this will replace the category/activity if it is found
                                    << "then" << open_document
                                        << "array" << open_document
                                            << "$concatArrays" << open_array
                                                << "$$value.array"
                                                << open_array
                                                    << activity_or_category_doc_view
                                                << close_array
                                            << close_array
                                        << close_document
                                        << "value_found" << true
                                    << close_document

                                    //else: increment current_index_num and add array element to return array
                                    // this will add on the non-matching category/activity element
                                    << "else" << open_document

                                        << "array" << open_document
                                            << "$concatArrays" << open_array
                                                << "$$value.array"
                                                << open_array
                                                    << "$$this"
                                                << close_array
                                            << close_array
                                        << close_document
                                        << "value_found" << "$$value.value_found"

                                    << close_document

                                << close_document
                            << close_document

                        << close_document

                    << close_document
                << close_document

                << "in" << open_document
                    << "$cond" << open_document

                        //if passed value was found
                        << "if" << "$$reduce_result.value_found"

                        //array element was updated, return the generated array
                        << "then" << "$$reduce_result.array"

                        //array element was NOT updated, need to concat the element
                        << "else" << open_document
                            << "$concatArrays" << open_array
                                << "$$reduce_result.array"
                                << open_array
                                    << activity_or_category_doc_view
                                << close_array
                            << close_array
                        << close_document

                    << close_document
                << close_document

            << close_document

        << close_document
    << finalize;

}

bsoncxx::document::value buildFindOutdatedIconIndexes(
        const google::protobuf::RepeatedField<google::protobuf::int64>& iconTimestamps
        ) {

    bsoncxx::builder::basic::array passedTimestamps;
    for(const auto& i : iconTimestamps) {
        passedTimestamps.append(bsoncxx::types::b_int64{i});
    }

    return document{}
        << "$expr" << open_document

            << "$let" << open_document
                << "vars" << open_document
                    << "storedArray" << passedTimestamps.view()
                << close_document
                << "in" << open_document

                    << "$cond" << open_document
                        << "if" << open_document

                            //Short-circuiting does not work inside the aggregation pipeline. This means that it is
                            // impossible to check the array size before the $arrayElemAt and have it be meaningful
                            // in any way. However, $arrayElemAt can be used alone.
                            //The way $gt works is that the long will be greater than the result of $arrayElemAt
                            // if it is outside the range of the array (the operation will not error) because it
                            // will compare by type and type 'long' > type 'missing'.
                            << "$gt" << open_array
                                << open_document
                                    << "$toLong" << "$" + icons_info_keys::TIMESTAMP_LAST_UPDATED
                                << close_document
                                << open_document
                                    << "$arrayElemAt" << open_array
                                        << "$$storedArray" << "$_id"
                                    << close_array
                                << close_document
                            << close_array

                        << close_document
                        << "then" << true
                        << "else" << false
                    << close_document

                << close_document
            << close_document

        << close_document
    << finalize;

}

bsoncxx::document::value buildProjectActivitiesByIndex(
        const std::string& PROJECTED_ARRAY_NAME,
        const bsoncxx::array::view& index_numbers_array_view,
        const int user_age
) {

    return document{}
            << PROJECTED_ARRAY_NAME << open_document

                << "$let" << open_document
                    << "vars" << open_document

                        //act_arr will end up as an array of elements that are either
                        // 1) a document containing MIN_AGE & CATEGORY_INDEX
                        // 2) null
                        //it will end up the same size as index_numbers_array_view
                        << "act_arr" << open_document
                            << "$map" << open_document
                                << "input" << index_numbers_array_view
                                << "in" << open_document
                                    << "$let" << open_document

                                        //save a var $$elem to represent the activity index element
                                        << "vars" << open_document
                                            << "elem" << open_document
                                                << "$arrayElemAt" << open_array
                                                    << "$" + activities_info_keys::ACTIVITIES << "$$this"
                                                << close_array
                                            << close_document
                                        << close_document

                                        << "in" << open_document
                                            << "$cond" << open_document

                                                //if: activity index exists inside array
                                                << "if" << open_document
                                                    << "$ne" << open_array
                                                        << open_document
                                                            << "$type" << "$$elem"
                                                        << close_document
                                                        << "missing"
                                                    << close_array
                                                << close_document

                                                //then: check user age; NOTE: Mongodb does not support short-circuiting with $and,
                                                // so this is a nested condition.
                                                << "then" << open_document
                                                    << "$cond" << open_document

                                                        //if: user is inside allowed age range
                                                        << "if" << open_document
                                                            << "$gte" << open_array
                                                                << user_age
                                                                << "$$elem." + activities_info_keys::activities::MIN_AGE
                                                            << close_array
                                                        << close_document

                                                        //then: project the necessary array element fields
                                                        << "then" << open_document
                                                            << activities_info_keys::activities::MIN_AGE << "$$elem." + activities_info_keys::activities::MIN_AGE
                                                            << activities_info_keys::activities::CATEGORY_INDEX << "$$elem." + activities_info_keys::activities::CATEGORY_INDEX
                                                        << close_document

                                                        //else: set the array element to null
                                                        << "else" << bsoncxx::types::b_null{}

                                                    << close_document
                                                << close_document

                                                //else: set the array element to null
                                                << "else" << bsoncxx::types::b_null{}

                                            << close_document
                                        << close_document

                                    << close_document
                                << close_document
                            << close_document
                        << close_document

                    << close_document
                    << "in" << open_document

                        << "$map" << open_document
                            << "input" << "$$act_arr"
                            << "in" << open_document

                                << "$cond" << open_document

                                    //if this element is not already set to null
                                    << "if" << open_document
                                        << "$ne" << open_array
                                            << open_document
                                                << "$type" << "$$this"
                                            << close_document
                                            << "null"
                                        << close_array
                                    << close_document

                                    //then: check categories min age
                                    << "then" << open_document

                                        << "$let" << open_document

                                            //save a var $$elem to represent the category index element
                                            << "vars" << open_document
                                                << "elem" << open_document
                                                    << "$arrayElemAt" << open_array
                                                        << "$" + activities_info_keys::CATEGORIES
                                                        << "$$this." + activities_info_keys::activities::CATEGORY_INDEX
                                                    << close_array
                                                << close_document
                                            << close_document

                                            << "in" << open_document
                                                << "$cond" << open_document

                                                    //if: category index exists inside array
                                                    << "if" << open_document
                                                        << "$ne" << open_array
                                                            << open_document
                                                                << "$type" << "$$elem"
                                                            << close_document
                                                            << "missing"
                                                        << close_array
                                                    << close_document

                                                    //then: check user age; NOTE: Mongodb does not support short-circuiting with $and,
                                                    // so this is a nested condition.
                                                    << "then" << open_document
                                                        << "$cond" << open_document

                                                            //if: user is inside allowed age range
                                                            << "if" << open_document
                                                                << "$gte" << open_array
                                                                    << user_age
                                                                    << "$$elem." + activities_info_keys::categories::MIN_AGE
                                                                << close_array
                                                            << close_document

                                                            //then: project the min age element fields
                                                            << "then" << open_document
                                                                << activities_info_keys::activities::CATEGORY_INDEX << "$$this." + activities_info_keys::activities::CATEGORY_INDEX
                                                                << "type" <<  open_document
                                                                    << "$type" << "$$elem"
                                                                << close_document
                                                            << close_document

                                                            //else: set the array element to null
                                                            << "else" << bsoncxx::types::b_null{}

                                                        << close_document
                                                    << close_document

                                                    //else: set the array element to null
                                                    << "else" << bsoncxx::types::b_null{}

                                                << close_document
                                            << close_document

                                        << close_document

                                    << close_document

                                    //else: returns the null object
                                    << "else" << "$$this"

                                << close_document

                            << close_document
                        << close_document

                    << close_document
                << close_document

            << close_document
        << finalize;



}

bsoncxx::document::value buildUpdateAccountsToMatchMade(
        const std::string& chat_room_id,
        const bsoncxx::types::b_date& last_time_viewed_date_object,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& match_account_oid
) {

    static const std::string MATCHED_OID_VARIABLE = "mato";
    static const std::string MATCHED_OID_VARIABLE_REFERENCE = "$$" + MATCHED_OID_VARIABLE;

    return document{}
            << user_account_keys::CHAT_ROOMS << open_document
                << "$concatArrays" << open_array
                    << "$" + user_account_keys::CHAT_ROOMS
                    << open_array
                        << open_document
                            << user_account_keys::chat_rooms::CHAT_ROOM_ID << chat_room_id
                            << user_account_keys::chat_rooms::LAST_TIME_VIEWED << last_time_viewed_date_object
                        << close_document
                    << close_array
                << close_array
            << close_document
            << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_document

                << "$concatArrays" << open_array
                    << "$" + user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH

                    //returns empty array if oid already exists in OTHER_ACCOUNTS_MATCHED_WITH
                    // otherwise returns an array containing the element to be concatenated
                    << open_document
                        << "$let" << open_document

                            << "vars" << open_document
                                //save the matched account OID to a variable to avoid the condition running every time
                                << MATCHED_OID_VARIABLE << open_document
                                    << "$cond" << open_document
                                        << "if" << open_document
                                            << "$eq" << open_array
                                                << "$_id" << user_account_oid
                                            << close_array
                                        << close_document
                                        << "then" << match_account_oid.to_string()
                                        << "else" << user_account_oid.to_string()
                                    << close_document
                                << close_document

                            << close_document
                            << "in" << open_document

                                << "$reduce" << open_document
                                    << "input" << "$" + user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH
                                    //initial value is an array containing the element to be concatenated
                                    << "initialValue" << open_array
                                        << open_document
                                            << user_account_keys::other_accounts_matched_with::OID_STRING << MATCHED_OID_VARIABLE_REFERENCE
                                            << user_account_keys::other_accounts_matched_with::TIMESTAMP << last_time_viewed_date_object
                                        << close_document
                                    << close_array

                                    //If the user already exists inside the array, return an empty array to be concatenated (it will add
                                    // nothing to OTHER_ACCOUNTS_MATCHED_WITH).
                                    << "in" << open_document
                                        << "$cond" << open_document

                                            << "if" << open_document
                                                << "$eq" << open_array
                                                    << "$$this." + user_account_keys::other_accounts_matched_with::OID_STRING << MATCHED_OID_VARIABLE_REFERENCE
                                                << close_array
                                            << close_document
                                            << "then" << open_array << close_array
                                            << "else" << "$$value"

                                        << close_document
                                    << close_document
                                << close_document

                            << close_document

                        << close_document
                    << close_document

                << close_array

            << close_document
        << finalize;

}

//checks if user is a member of chat room
bsoncxx::document::value buildUserRequiredInChatRoomCondition(const std::string& chatRoomId) {

            return document{}
                    << "$reduce" << open_document
                        << "input" << "$" + user_account_keys::CHAT_ROOMS
                        << "initialValue" << false
                        << "in" << open_document

                            << "$cond" << open_document

                                //if the chat room is the chat room ID
                                << "if" << open_document

                                    << "$eq" << open_array
                                        << "$$this." + user_account_keys::chat_rooms::CHAT_ROOM_ID
                                        << chatRoomId
                                    << close_array

                                << close_document

                                << "then" << true
                                << "else" << "$$value"
                            << close_document
                        << close_document
                    << close_document
                    << finalize;

}

template<typename T>
bsoncxx::document::value buildUserLeftChatRoomHeaderCondition(const bsoncxx::array::view& concatArrayElement, const T& setAdminValue) {

    return document{}
        << "arrayVal" << open_document
            << "$concatArrays" << open_array
                << "$$value.arrayVal"
                << concatArrayElement
            << close_array
        << close_document
        << "adminIsSet" << setAdminValue
    << finalize;

}

bsoncxx::document::value extractUserAccountState(const bsoncxx::oid& user_account_oid) {

    return document{}
        << "$reduce" << open_document

            << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
            << "initialValue" << bsoncxx::types::b_int32{-1}
            << "in" << open_document

                << "$cond" << open_document

                    //if: this element contains the userAccountOID
                    << "if" << open_document
                        << "$eq" << open_array
                            << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                        << close_array
                    << close_document

                    //then: set
                    << "then" << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM

                    //else: return the value
                    << "else" << "$$value"

                << close_document
            << close_document

        << close_document
    << finalize;

}

bsoncxx::document::value buildUserLeftChatRoomHeader(
        const bsoncxx::oid& userAccountOID,
        const int thumbnail_size,
        const std::string& thumbnail_reference_oid,
        const std::chrono::milliseconds& currentTimestamp
) {

    const auto mongoDBDate = bsoncxx::types::b_date{ currentTimestamp };

    bsoncxx::document::value updateUserAccountDoc = document{}
            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
            << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << "$$this." + chat_room_header_keys::accounts_in_chat_room::FIRST_NAME
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << mongoDBDate
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << open_document
                << "$literal" << thumbnail_reference_oid
            << close_document
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << thumbnail_size
            << chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << open_document
                << "$max" << open_array
                    << mongoDBDate << "$$this." + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME
                << close_array
            << close_document
            << chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << open_document
                << "$concatArrays" << open_array
                    << "$$this." + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT
                    << open_array
                        << mongoDBDate
                    << close_array
                << close_array
            << close_document
        << finalize;

    bsoncxx::builder::basic::array updateUserAccountArray;
    updateUserAccountArray.append(updateUserAccountDoc);

    bsoncxx::builder::basic::array upgradeAccountStateToAdminArray;

    upgradeAccountStateToAdminArray.append(
        document{}
            << "$mergeObjects" << open_array
                << "$$this"
                << open_document
                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                << close_document
            << close_array
        << finalize
    );

    bsoncxx::builder::basic::array thisObjectArray;
    thisObjectArray.append("$$this");

    return document{}

        //update header last active time
        << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << open_document
            << "$max" << open_array
                << "$" + chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongoDBDate
            << close_array
        << close_document

        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
            << "$let" << open_document

                //get user account state and save it to $$accountState, if -1 user account does not exist in chat room
                << "vars" << open_document
                    << "accountState" << extractUserAccountState(userAccountOID)
                << close_document

                //after variable has been saved to $$accountState, set up new admin
                // and save user account info
                << "in" << open_document

                    << "$switch" << open_document

                        << "branches" << open_array

                            //current user AccountStateInChatRoom::STATE_IN_CHAT_ROOM
                            << open_document
                                << "case" << open_document
                                    << "$eq" << open_array
                                        << "$$accountState" << AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                    << close_array
                                << close_document

                                << "then" << open_document

                                    << "$map" << open_document
                                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                        << "in" << open_document
                                            << "$cond" << open_document

                                                //if: this element contains the userAccountOID
                                                << "if" << open_document
                                                    << "$eq" << open_array
                                                        << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                                                    << close_array
                                                << close_document

                                                //then: update the element to the new values
                                                << "then" << updateUserAccountDoc.view()

                                                //else: return the same element
                                                << "else" << "$$this"

                                            << close_document
                                        << close_document
                                    << close_document

                                << close_document
                            << close_document

                            //current user AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                            << open_document
                                << "case" << open_document
                                    << "$eq" << open_array
                                        << "$$accountState" << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                    << close_array
                                << close_document

                                << "then" << open_document

                                    << "$let" << open_document

                                        //get the $reduce results and save them to a variable
                                        << "vars" << open_document
                                            << "reducedValue" << open_document

                                                //iterate through and find a new admin to set if a valid user exists
                                                << "$reduce" << open_document
                                                    << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                                    << "initialValue" << open_document
                                                        << "arrayVal" << open_array
                                                        << close_array
                                                        << "adminIsSet" << false
                                                    << close_document
                                                    << "in" << open_document

                                                        << "$switch" << open_document
                                                            << "branches" << open_array

                                                                << open_document
                                                                    //if: account is current user
                                                                    << "case" << open_document
                                                                        << "$eq" << open_array
                                                                            << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                                                                        << close_array
                                                                    << close_document
                                                                    //then: set the account values
                                                                    << "then" << buildUserLeftChatRoomHeaderCondition(updateUserAccountArray.view(), "$$value.adminIsSet")

                                                                << close_document

                                                                << open_document
                                                                    //if: admin is not yet set and account is viable to become admin
                                                                    << "case" << open_document
                                                                        << "$and" << open_array
                                                                            << open_document
                                                                                << "$eq" << open_array
                                                                                    << "$$value.adminIsSet" << false
                                                                                << close_array
                                                                            << close_document
                                                                            << open_document
                                                                                << "$or" << open_array
                                                                                    << open_document
                                                                                        << "$eq" << open_array
                                                                                            << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                                                                        << close_array
                                                                                    << close_document
                                                                                    << open_document
                                                                                        << "$eq" << open_array
                                                                                            << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                                                                        << close_array
                                                                                    << close_document
                                                                                << close_array
                                                                            << close_document
                                                                        << close_array
                                                                    << close_document

                                                                    //then: set this account to admin
                                                                    << "then" << buildUserLeftChatRoomHeaderCondition(upgradeAccountStateToAdminArray.view(), true)

                                                                << close_document

                                                            << close_array
                                                            //if: account is not current user and is not viable to become admin OR admin was already set
                                                            //then: do not change anything, simple concat the array on
                                                            << "default" << buildUserLeftChatRoomHeaderCondition(thisObjectArray.view(), "$$value.adminIsSet")

                                                        << close_document

                                                    << close_document
                                                << close_document

                                            << close_document
                                        << close_document

                                        << "in" << "$$reducedValue.arrayVal"

                                    << close_document

                                << close_document
                            << close_document

                        << close_array

                        //if: user was not found or has a different account state
                        //then: do no updates
                        << "default" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM

                    << close_document

                << close_document

            << close_document
        << close_document

        << finalize;

}

mongocxx::pipeline buildUserLeftChatRoomExtractAccountsAggregation(
        const bsoncxx::oid& user_account_oid
) {

    bsoncxx::document::value projection_doc = document{}
            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
            << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << "$$this." + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE
        << finalize;

    mongocxx::pipeline pipeline;
    pipeline.match(
        document{}
            << "_id" << chat_room_header_keys::ID
            << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
        << finalize
    );

    pipeline.project(
        document{}
            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                << "$let" << open_document

                    //get user account state and save it to $$accountState, if -1 user account does not exist in chat room
                    << "vars" << open_document
                        << "accountState" << extractUserAccountState(user_account_oid)
                    << close_document

                    //after variable has been saved to $$accountState, set up new admin
                    // and save user account info
                    << "in" << open_document

                        << "$switch" << open_document

                            << "branches" << open_array

                                //current user AccountStateInChatRoom::STATE_IN_CHAT_ROOM
                                << open_document
                                    << "case" << open_document
                                        << "$eq" << open_array
                                            << "$$accountState" << AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                        << close_array
                                    << close_document

                                    << "then" << open_document

                                        << "$reduce" << open_document
                                            << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                            << "initialValue" << open_array << close_array
                                            << "in" << open_document
                                                << "$cond" << open_document

                                                    //if: this element contains the user_account_oid
                                                    << "if" << open_document
                                                        << "$eq" << open_array
                                                            << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                                        << close_array
                                                    << close_document

                                                    //then: extract the necessary user values
                                                    << "then" << open_array
                                                        << projection_doc
                                                    << close_array

                                                    //else: return the same element
                                                    << "else" << "$$value"

                                                << close_document
                                            << close_document
                                        << close_document

                                    << close_document
                                << close_document

                                //current user AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                << open_document
                                    << "case" << open_document
                                        << "$eq" << open_array
                                            << "$$accountState" << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                        << close_array
                                    << close_document

                                    << "then" << open_document

                                        << "$let" << open_document

                                            //get the $reduce results and save them to a variable
                                            << "vars" << open_document
                                                << "reducedValue" << open_document

                                                    //iterate through and find a new admin to set if a valid user exists
                                                    << "$reduce" << open_document
                                                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                                        << "initialValue" << open_document
                                                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_array << close_array
                                                            << "adminIsSet" << false
                                                        << close_document
                                                        << "in" << open_document

                                                            << "$switch" << open_document
                                                                << "branches" << open_array

                                                                    << open_document

                                                                        //if: account is current user
                                                                        << "case" << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                                                            << close_array
                                                                        << close_document

                                                                        //then: set the account values
                                                                        << "then" << open_document
                                                                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                                                                                << "$concatArrays" << open_array
                                                                                    << "$$value." + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                                                                    << open_array
                                                                                        << projection_doc
                                                                                    << close_array
                                                                                << close_array
                                                                            << close_document
                                                                            << "adminIsSet" << "$$value.adminIsSet"
                                                                        << close_document

                                                                    << close_document

                                                                    << open_document
                                                                        //if: admin has not yet been found and account is viable to become admin
                                                                        << "case" << open_document
                                                                            << "$and" << open_array
                                                                                << open_document
                                                                                    << "$eq" << open_array
                                                                                        << "$$value.adminIsSet" << false
                                                                                    << close_array
                                                                                << close_document
                                                                                << open_document
                                                                                    << "$or" << open_array
                                                                                        << open_document
                                                                                            << "$eq" << open_array
                                                                                                << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                                                                            << close_array
                                                                                        << close_document
                                                                                        << open_document
                                                                                            << "$eq" << open_array
                                                                                                << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                                                                            << close_array
                                                                                        << close_document
                                                                                    << close_array
                                                                                << close_document
                                                                            << close_array
                                                                        << close_document

                                                                        //then: this account will be made the account admin
                                                                        << "then" << open_document
                                                                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                                                                                << "$concatArrays" << open_array
                                                                                    << "$$value." + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                                                                    << open_array
                                                                                        << projection_doc
                                                                                    << close_array
                                                                                << close_array
                                                                            << close_document
                                                                            << "adminIsSet" << true
                                                                        << close_document
                                                                    << close_document

                                                                << close_array
                                                                //if: account is not current user and is not viable to become admin OR admin was already set
                                                                //then: do not change anything
                                                                << "default" << "$$value"

                                                            << close_document

                                                        << close_document
                                                    << close_document

                                                << close_document
                                            << close_document

                                            << "in" <<  "$$reducedValue." + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM

                                        << close_document

                                    << close_document
                                << close_document

                            << close_array

                            //if: user was not found or has a different account state
                            //then: return an empty document
                            << "default" << open_array << close_array

                        << close_document

                    << close_document

                << close_document
            << close_document

        << finalize
    );

    return pipeline;
}

bsoncxx::document::value matchChatRoomUserInside(const bsoncxx::oid& userAccountOID) {
    return document{}
        << "_id" << chat_room_header_keys::ID
        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
            << "$elemMatch" << open_document
                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                << "$or" << open_array
                    << open_document
                        << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << (int) AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                    << close_document
                    << open_document
                        << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << (int) AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                    << close_document
                << close_array
            << close_document
        << close_document
        << finalize;
}

/** Something similar to these values is used separately inside streamInitializationMessagesToClient.cpp **/
bsoncxx::document::value buildMessageIsDeleteTypeDocument() {
    return document{}
            << "$in" << open_array
                << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                << (int)MessageSpecifics::MessageBodyCase::kPictureMessage
                << (int)MessageSpecifics::MessageBodyCase::kLocationMessage
                << (int)MessageSpecifics::MessageBodyCase::kMimeTypeMessage
                << (int)MessageSpecifics::MessageBodyCase::kInviteMessage
            << close_array
        << finalize;
}

bsoncxx::document::value buildUpdateChatRoomHeaderLastActiveTimeForUser(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
) {
   return document{}
        << "$mergeObjects" << open_array
            << "$$ROOT"
            << open_document
                << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                    << "$map" << open_document
                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                        << "as" << "users"
                        << "in" << open_document
                            << "$cond" << open_document

                                << "if" << open_document
                                    << "$eq" << open_array
                                        << "$$users." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                                    << close_array
                                << close_document

                                //if this is the current users document
                                << "then" << open_document
                                    << "$mergeObjects" << open_array
                                        << "$$users"
                                        << open_document

                                            //set the last activity time to the max possible value
                                            << chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << open_document
                                                << "$max" << open_array
                                                    << "$$users." + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME
                                                    << bsoncxx::types::b_date{currentTimestamp}
                                                << close_array
                                            << close_document

                                        << close_document
                                    << close_array
                                << close_document

                                //if this is not the current users document
                                << "else" << "$$users"

                            << close_document
                        << close_document
                    << close_document
                << close_document
            << close_document
        << close_array
    << finalize;
}


bsoncxx::document::value buildClientMessageToServerDeleteForEveryoneProjectionDocument(const bsoncxx::oid& userAccountOID) {
    return document{}
    << "newRoot" << open_document
        << "$cond" << open_document

            //if: is header _id
            << "if" << open_document
                << "$eq" << open_array
                    << "$_id" << chat_room_header_keys::ID
                << close_array
            << close_document

            //then: this is header document
            << "then" << open_document
                << "_id" << "$_id"
                << chat_room_header_keys::MATCHING_OID_STRINGS << "$" + chat_room_header_keys::MATCHING_OID_STRINGS
                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << open_document
                    << "$reduce" << open_document
                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                        << "initialValue" << AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                        << "in" << open_document
                            << "$cond" << open_document

                                //if: this element is from the current user
                                << "if" << open_document
                                    << "$eq" << open_array
                                        << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                        << userAccountOID
                                    << close_array
                                << close_document

                                //then: save the element
                                << "then" << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM

                                //else: do nothing
                                << "else" << "$$value"

                            << close_document
                        << close_document
                    << close_document
                << close_document
            << close_document

            //else: this is message document
            << "else" << open_document
                << "_id" << "$_id"
                << chat_room_message_keys::MESSAGE_SENT_BY << "$" + chat_room_message_keys::MESSAGE_SENT_BY
                << chat_room_shared_keys::TIMESTAMP_CREATED << "$" + chat_room_shared_keys::TIMESTAMP_CREATED
            << close_document

        << close_document
    << close_document
    << finalize;
}

bsoncxx::document::value buildClientMessageToServerDeleteForEveryoneUpdateDocument(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
) {
    return document{}
        << "newRoot" << open_document
            << "$cond" << open_document

                //if: is header _id
                << "if" << open_document
                    << "$eq" << open_array
                        << "$_id" << chat_room_header_keys::ID
                    << close_array
                << close_document

                //then: this is header document
                << "then" << buildUpdateChatRoomHeaderLastActiveTimeForUser(userAccountOID,currentTimestamp)

                //else: this is message document
                << "else" << open_document
                    << "$mergeObjects" << open_array
                        << "$$ROOT"
                        << open_document
                            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                                << "$mergeObjects" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT
                                    << open_document
                                        //set the 'delete type' to DELETE_FOR_ALL_USERS
                                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
                                            << "$mergeObjects" << open_array
                                                << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT
                                                << open_document
                                                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_FOR_ALL_USERS
                                                << close_document
                                            << close_array
                                        << close_document
                                    << close_document
                                << close_array
                            << close_document
                        << close_document
                    << close_array
                << close_document
            << close_document

        << close_document
    << finalize;
}

bsoncxx::document::value buildClientMessageToServerDeleteSingleUserUpdateDocument(
        const std::string& userAccountOIDStr
) {
    return document{}
        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << "$mergeObjects" << open_array
                << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT
                << open_document
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << open_document
                        << "$cond" << open_document

                            //if: deleted_type is set to all users
                            << "if" << open_document
                                << "$eq" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY
                                    << DeleteType::DELETE_FOR_ALL_USERS
                                << close_array
                            << close_document

                            //then: leave delete type the same
                            << "then" << DeleteType::DELETE_FOR_ALL_USERS

                            //else: change it to delete for single user
                            << "else" << DeleteType::DELETE_FOR_SINGLE_USER

                        << close_document
                    << close_document
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_document
                        << "$cond" << open_document

                            //if: user account oid exists inside array
                            << "if" << open_document
                                << "$in" << open_array
                                    << userAccountOIDStr
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY
                                << close_array
                            << close_document

                            //then: return array without changes
                            << "then" << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY

                            //else: add the user oid as a string to the array
                            << "else" << open_document
                                << "$concatArrays" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY
                                    << open_array
                                        << userAccountOIDStr
                                    << close_array
                                << close_array
                            << close_document

                        << close_document
                    << close_document
                << close_document
            << close_array
        << close_document
    << finalize;
}

bsoncxx::document::value buildClientMessageToServerUpdateModifiedMessage(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& updateMessageDocument
) {
    return document{}
        << "newRoot" << open_document
            << "$cond" << open_document

                //if: is header _id
                << "if" << open_document
                    << "$eq" << open_array
                        << "$_id" << chat_room_header_keys::ID
                    << close_array
                << close_document

                //then: this is header document
                << "then" << buildUpdateChatRoomHeaderLastActiveTimeForUser(userAccountOID,currentTimestamp)

                //if message document
                << "else" << open_document
                    << "$mergeObjects" << open_array
                        << "$$ROOT"
                        << open_document
                            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                                << "$mergeObjects" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT
                                    << updateMessageDocument
                                << close_array
                            << close_document
                        << close_document
                    << close_array
                << close_document
            << close_document

        << close_document
    << finalize;
}

bsoncxx::document::value buildCheckIfAccountDeletedDocument(const std::string& currentUserAccountOIDStr) {
    return document{}
            << "$or" << open_array

            //delete type is equal to DELETE_TYPE_NOT_SET
            << open_document
                << "$eq" << open_array
                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY
                    << DeleteType::DELETE_TYPE_NOT_SET
                << close_array
            << close_document

            //delete type is equal to DELETE_FOR_SINGLE_USER however user is not in deleted list
            << open_document
                << "$and" << open_array

                    << open_document
                        << "$eq" << open_array
                            << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY
                            << DeleteType::DELETE_FOR_SINGLE_USER
                        << close_array
                    << close_document

                    << open_document
                        << "$not" << open_array
                            << open_document
                                << "$in" << open_array
                                    << currentUserAccountOIDStr
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY
                                << close_array
                            << close_document
                        << close_array
                    << close_document

                << close_array
            << close_document

        << close_array
    << finalize;
}

bsoncxx::document::value buildSortByIndexDoc(
        const std::string& ARRAY_ID_KEY,
        const std::string& ARRAY_INDEX_KEY,
        const bsoncxx::array::view& user_oid_to_index
        ) {

    return bsoncxx::builder::stream::document{}
        << "$reduce" << open_document

            //reduce this to a single int for sorting by index
            << "input" << user_oid_to_index
            << "initialValue" << INT_MAX
            << "in" << open_document
                << "$cond" << open_document

                    //if: this is the array value for the current chat room uuid
                    << "if" << open_document
                        << "$eq" << open_array
                            << "$$this." + ARRAY_ID_KEY
                            << "$_id"
                        << close_array
                    << close_document

                    //then: save found index
                    << "then" << "$$this." + ARRAY_INDEX_KEY

                    //else: save previously stored value
                    << "else" << "$$value"

                << close_document
            << close_document
        << close_document
    << finalize;

}

bsoncxx::document::value buildCheckIfUserIsInHeaderAndKeepMessagesTheSame(
        const bsoncxx::oid& userAccountOID,
        const std::string& MESSAGE_TYPE_VALID_KEY,
        const std::string& SORTING_BY_INDEX_KEY,
        const std::string& ARRAY_ID_KEY,
        const std::string& ARRAY_INDEX_KEY,
        const bsoncxx::array::view& user_oid_to_index
) {
    return document{}
        << "newRoot" << open_document
            << "$cond" << open_document

                //if _id is header id
                << "if" << open_document
                    << "$eq" << open_array
                        << "$_id" << chat_room_header_keys::ID
                    << close_array
                << close_document

                //then: this is the header
                << "then" << open_document
                    << "$reduce" << open_document

                        //reduce this to a single document that has a value "found" and a bool true or false
                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                        << "initialValue" << open_document
                            << "_id" << "$_id"
                            << chat_room_shared_keys::TIMESTAMP_CREATED << "$" + chat_room_shared_keys::TIMESTAMP_CREATED
                            << "found" << false
                            << SORTING_BY_INDEX_KEY << -1
                        << close_document
                        << "in" << open_document
                            << "$cond" << open_document

                                //if: this is the current user id and the user is inside the chat room
                                << "if" << open_document
                                    << "$and" << open_array
                                        << open_document
                                            << "$eq" << open_array
                                                << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                                            << close_array
                                        << close_document
                                        << open_document
                                            << "$in" << open_array
                                                << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                                << open_array
                                                    << AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                                                    << AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                                << close_array
                                            << close_array
                                        << close_document
                                    << close_array
                                << close_document

                                //then: save found to true
                                << "then" << open_document
                                    << "$mergeObjects" << open_array
                                        << "$$value"
                                        << open_document
                                            << "found" << true
                                            << SORTING_BY_INDEX_KEY << -1
                                        << close_document
                                    << close_array
                                << close_document

                                //else: save previously stored value
                                << "else" << "$$value"

                            << close_document
                        << close_document

                    << close_document
                << close_document

                //else: this is a normal message
                << "else" << open_document
                    << "$cond" << open_document

                        //if: this is an 'active' message type
                        << "if" << open_document
                            << "$in" << open_array
                                << "$" + chat_room_message_keys::MESSAGE_TYPE
                                << buildActiveMessageTypesDoc()
                            << close_array
                        << close_document

                        //then: save entire document
                        << "then" << open_document
                            << "$mergeObjects" << open_array
                                << "$$ROOT"
                                << open_document
                                    << MESSAGE_TYPE_VALID_KEY << true
                                    << SORTING_BY_INDEX_KEY << buildSortByIndexDoc(ARRAY_ID_KEY, ARRAY_INDEX_KEY, user_oid_to_index)
                                << close_document
                            << close_array
                        << close_document

                        //else: save minimal info
                        << "else" << open_document
                            << "_id" << "$_id"
                            << MESSAGE_TYPE_VALID_KEY << false
                            << SORTING_BY_INDEX_KEY << buildSortByIndexDoc(ARRAY_ID_KEY, ARRAY_INDEX_KEY, user_oid_to_index)
                        << close_document

                    << close_document

                << close_document

            << close_document
        << close_document
    << finalize;
}

bsoncxx::document::value buildUpdateReportLogDocument(
        const bsoncxx::oid& reported_user_account_oid,
        const bsoncxx::oid& reporting_user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::document::view& report_log_element
        ) {

    bsoncxx::builder::basic::array timestamp_limit_reached_branches;

    if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED > 1) {
        timestamp_limit_reached_branches.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << open_document
                            << "$type" << "$" + outstanding_reports_keys::REPORTS_LOG
                        << close_document
                        << "missing"
                    << close_array
                << close_document
                << "then" << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
            << finalize
        );

        timestamp_limit_reached_branches.append(
            document{}
                << "case" << open_document
                    << "$and" << open_array

                        //REPORTS_LOG is correct size for TIMESTAMP_LIMIT_REACHED to be set
                        << open_document
                            << "$eq" << open_array
                                << open_document
                                    << "$size" << "$" + outstanding_reports_keys::REPORTS_LOG
                                << close_document
                                << report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED - 1 //minus 1 is because a new one is being added by this update
                            << close_array
                        << close_document

                        //element will be added to REPORTS_LOG
                        << open_document

                            //if this document will be added below to REPORTS_LOG
                            << "$reduce" << open_document
                                << "input" << "$" + outstanding_reports_keys::REPORTS_LOG
                                << "initialValue" << true
                                << "in" << open_document
                                    << "$cond" << open_document

                                        //if this element was also sent by the reporting user, return false
                                        << "if" << open_document
                                            << "$eq" << open_array
                                                << reporting_user_account_oid << std::string("$$this.").append(outstanding_reports_keys::reports_log::ACCOUNT_OID)
                                            << close_array
                                        << close_document
                                        << "then" << false
                                        << "else" << "$$value"

                                    << close_document
                                << close_document
                            << close_document

                        << close_document

                    << close_array
                << close_document
                << "then" << bsoncxx::types::b_date{current_timestamp}
            << finalize
        );
    } else { //If only 1 report required to make it available for admins to view.
        timestamp_limit_reached_branches.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << open_document
                            << "$type" << "$" + outstanding_reports_keys::REPORTS_LOG
                        << close_document
                        << "missing"
                    << close_array
                << close_document
                << "then" << bsoncxx::types::b_date{current_timestamp}
            << finalize
        );
    }

    return document{}
        << "_id" << reported_user_account_oid
        << outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED << open_document

            << "$switch" << open_document
                << "branches" << timestamp_limit_reached_branches
                << "default" << "$" + outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED
            << close_document

        << close_document

        << outstanding_reports_keys::REPORTS_LOG << open_document

            << "$cond" << open_document
                //if the document exists, REPORTS_LOG will exist
                << "if" << open_document
                    << "$eq" << open_array
                        << open_document
                            << "$type" << "$" + outstanding_reports_keys::REPORTS_LOG
                        << close_document
                        << "array"
                    << close_array
                << close_document
                << "then" << open_document

                    << "$let" << open_document
                        << "vars" << open_document

                            //if this user already reported the other user once
                            << "user_exists" << open_document
                                << "$reduce" << open_document
                                    << "input" << "$" + outstanding_reports_keys::REPORTS_LOG
                                    << "initialValue" << false
                                    << "in" << open_document
                                        << "$cond" << open_document

                                            //if this element was also sent by the reporting user, return true
                                            << "if" << open_document
                                                << "$eq" << open_array
                                                    << reporting_user_account_oid << std::string("$$this.").append(outstanding_reports_keys::reports_log::ACCOUNT_OID)
                                                << close_array
                                            << close_document
                                            << "then" << true
                                            << "else" << "$$value"

                                        << close_document
                                    << close_document
                                << close_document
                            << close_document

                        << close_document
                        << "in" << open_document
                            << "$cond" << open_document

                                //if this element was also sent by the reporting user, do not concatenate it
                                << "if" << "$$user_exists"
                                << "then" << "$" + outstanding_reports_keys::REPORTS_LOG
                                << "else" << open_document
                                    << "$concatArrays" << open_array
                                        << "$" + outstanding_reports_keys::REPORTS_LOG
                                        << open_array
                                            << report_log_element
                                        << close_array
                                    << close_array
                                << close_document

                            << close_document
                        << close_document
                    << close_document

                << close_document
                << "else" << open_array
                    << report_log_element
                << close_array
            << close_document

        << close_document

        << finalize;

}

bsoncxx::document::value buildCountTimesEachValueOccursDocument(
        const std::string& field_key,
        const std::string& name_of_field_key,
        const std::string& number_of_times_repeated_key
        ) {
    //NOTE: Array is expected to be sorted before this function is called (ascending or descending is irrelevant).
    return document{}

        << "$let" << open_document
            << "vars" << open_document

                << "arr" << open_document

                    << "$reduce" << open_document

                        //concat an element so that the final element being worked on is always added to the array
                        << "input" << "$" + field_key

                        << "initialValue" << open_document
                            << "ret_val" << open_array << close_array
                            << "current_ele" << open_document
                                << name_of_field_key << bsoncxx::types::b_null{} //must always be different that values passed in array
                                << number_of_times_repeated_key << bsoncxx::types::b_int32{0}
                            << close_document
                        << close_document

                        << "in" << open_document

                            << "$switch" << open_document

                                << "branches" << open_array

                                    //first element of array
                                    << open_document
                                        << "case" << open_document
                                            << "$eq" << open_array
                                                << std::string("$$value.current_ele.").append(name_of_field_key) << bsoncxx::types::b_null{}
                                            << close_array
                                        << close_document
                                        << "then" << open_document
                                            << "ret_val" << "$$value.ret_val"
                                            << "current_ele" << open_document
                                                << name_of_field_key << "$$this"
                                                << number_of_times_repeated_key << bsoncxx::types::b_int32{1}
                                            << close_document
                                        << close_document
                                    << close_document

                                    //current element same as previous element
                                    << open_document
                                        << "case" << open_document
                                            << "$eq" << open_array
                                                << std::string("$$value.current_ele.").append(name_of_field_key) << "$$this"
                                            << close_array
                                        << close_document
                                        << "then" << open_document
                                            << "ret_val" << "$$value.ret_val"
                                            << "current_ele" << open_document
                                                << name_of_field_key << std::string("$$value.current_ele.").append(name_of_field_key)
                                                << number_of_times_repeated_key << open_document
                                                    << "$add" << open_array
                                                        << std::string("$$value.current_ele.").append(number_of_times_repeated_key)
                                                        << bsoncxx::types::b_int32{1}
                                                    << close_array
                                                << close_document
                                            << close_document
                                        << close_document
                                    << close_document

                                << close_array

                                //not first element and previous element different than current element
                                << "default" << open_document
                                    << "ret_val" << open_document
                                        << "$concatArrays" << open_array
                                            << "$$value.ret_val"
                                            << open_array
                                                << "$$value.current_ele"
                                            << close_array
                                        << close_array
                                    << close_document
                                    << "current_ele" << open_document
                                        << name_of_field_key << "$$this"
                                        << number_of_times_repeated_key << bsoncxx::types::b_int32{1}
                                    << close_document
                                << close_document
                            << close_document

                        << close_document

                    << close_document

                << close_document

            << close_document

            << "in" << open_document

                << "$cond" << open_document

                    //if first element was never set
                    << "if" << open_document
                        << "$eq" << open_array
                            << std::string("$$arr.current_ele.").append(name_of_field_key) << bsoncxx::types::b_null{}
                        << close_array
                    << close_document

                    //return an empty array
                    << "then" << open_array << close_array

                    //append element to array and return array
                    << "else" << open_document
                        << "$concatArrays" << open_array
                            << "$$arr.ret_val"
                            << open_array
                                << "$$arr.current_ele"
                            << close_array
                        << close_array
                    << close_document
                << close_document

            << close_document

        << close_document

    << finalize;
}

template <const std::string& field_key, const std::string& field_type>
inline bsoncxx::document::value buildPushIfFieldExistsDoc() {
    return document{}

        << "$push" << open_document
            << "$cond" << open_document

                //if field exists
                << "if" << open_document
                    << "$eq" << open_array
                        << open_document
                            << "$type" << "$" + field_key
                        << close_document
                        << field_type
                    << close_array
                << close_document

                //then $push it to the array
                << "then" << "$" + field_key

                //then do not $push it to the array
                << "else" << "$$REMOVE"

            << close_document
        << close_document

        << finalize;
}

bsoncxx::document::value buildGroupSearchedErrorsDocument(
        const std::string& document_count_key
        ) {

    //listed here https://www.mongodb.com/docs/manual/reference/bson-types/
    // it seems that these should be inside the code somewhere, haven't found them though
    static const std::string BSONCXX_INT_TYPE =  "int";
    static const std::string BSONCXX_STRING_TYPE =  "string";

    return document{}
        << "_id" << open_document
            << "$concat" << open_array
                << open_document
                    << "$toString" << "$" + fresh_errors_keys::ERROR_ORIGIN
                << close_document
                << "_"
                << open_document
                    << "$toString" << "$" + fresh_errors_keys::VERSION_NUMBER
                << close_document
                << "_"
                << "$" + fresh_errors_keys::FILE_NAME
                << "_"
                << open_document
                    << "$toString" << "$" + fresh_errors_keys::LINE_NUMBER
                << close_document
            << close_array
        << close_document
        << document_count_key << open_document
            << "$sum" << 1 //NOTE: $count for group stage has a bug and doesn't always work on later version of mongoDB (like 4.2 and 4.4)
        << close_document
        << fresh_errors_keys::ERROR_ORIGIN << open_document
            << "$first" << "$" + fresh_errors_keys::ERROR_ORIGIN
        << close_document
        << fresh_errors_keys::VERSION_NUMBER << open_document
            << "$first" << "$" + fresh_errors_keys::VERSION_NUMBER
        << close_document
        << fresh_errors_keys::FILE_NAME << open_document
            << "$first" << "$" + fresh_errors_keys::FILE_NAME
        << close_document
        << fresh_errors_keys::LINE_NUMBER << open_document
            << "$first" << "$" + fresh_errors_keys::LINE_NUMBER
        << close_document
        << fresh_errors_keys::ERROR_URGENCY << open_document
            << "$first" << "$" + fresh_errors_keys::ERROR_URGENCY
        << close_document
        << fresh_errors_keys::TIMESTAMP_STORED << open_document
            << "$max" << "$" + fresh_errors_keys::TIMESTAMP_STORED
        << close_document
        << fresh_errors_keys::API_NUMBER << buildPushIfFieldExistsDoc<fresh_errors_keys::API_NUMBER, BSONCXX_INT_TYPE>()
        << fresh_errors_keys::DEVICE_NAME << buildPushIfFieldExistsDoc<fresh_errors_keys::DEVICE_NAME, BSONCXX_STRING_TYPE>()
    << finalize;
}

bsoncxx::document::value buildProjectSearchedErrorsDocument(
        const std::string& document_count_key,
        const std::string& name_of_field_key,
        const std::string& number_of_times_repeated_key
        ) {

        return document{}
            << document_count_key << 1
            << fresh_errors_keys::ERROR_ORIGIN << 1
            << fresh_errors_keys::VERSION_NUMBER << 1
            << fresh_errors_keys::FILE_NAME << 1
            << fresh_errors_keys::LINE_NUMBER << 1
            << fresh_errors_keys::ERROR_URGENCY << 1
            << fresh_errors_keys::TIMESTAMP_STORED << 1
            << fresh_errors_keys::API_NUMBER << buildCountTimesEachValueOccursDocument(
                fresh_errors_keys::API_NUMBER, name_of_field_key, number_of_times_repeated_key)
            << fresh_errors_keys::DEVICE_NAME << buildCountTimesEachValueOccursDocument(
                fresh_errors_keys::DEVICE_NAME, name_of_field_key, number_of_times_repeated_key)
        << finalize;
}

bsoncxx::document::value buildProjectExtractedErrorDocumentStatistics(
        const std::string& PROJECTED_EXTRACT_INTERMEDIATE_KEY,
        const std::string& PROJECTED_TOTAL_BYTES,
        const std::string& PROJECTED_EXTRACTED_BYTES,
        const std::string& PROJECTED_TOTAL_COUNT_KEY,
        const std::string& PROJECTED_EXTRACTED_COUNT_KEY,
        const long MAX_NUMBER_OF_BYTES
        ) {
    return document{}
        << PROJECTED_EXTRACT_INTERMEDIATE_KEY << open_document

            << "$let" << open_document
                << "vars" << open_document
                    << "res_doc" << open_document

                        << "$reduce" << open_document

                            << "initialValue" << open_document
                                << PROJECTED_TOTAL_BYTES << 0
                                << PROJECTED_EXTRACTED_BYTES << 0
                                << PROJECTED_TOTAL_COUNT_KEY << 0
                                << PROJECTED_EXTRACTED_COUNT_KEY << 0
                                << "stop" << false
                            << close_document
                            << "input" << "$" + PROJECTED_EXTRACT_INTERMEDIATE_KEY
                            << "in" << open_document

                                << "$let" << open_document
                                    << "vars" << open_document
                                        << "bytes_sum" << open_document
                                            << "$add" << open_array
                                                << "$$value." + PROJECTED_TOTAL_BYTES << "$$this"
                                            << close_array
                                        << close_document
                                        << "count_sum" << open_document
                                            << "$add" << open_array
                                                << "$$value." + PROJECTED_TOTAL_COUNT_KEY << 1
                                            << close_array
                                        << close_document
                                    << close_document
                                    << "in" << open_document

                                        << "$cond" << open_document

                                            //if cumulative value does not exceed max number of bytes AND
                                            //if value has not been reached (otherwise could go out of
                                            // order, say doc 42 is 500kb and brings it over the limit
                                            // however doc 43 is 12kb and doesn't bring it over. Because
                                            // this algorithm assumes order this cannot happen and the
                                            // stop bool will prevent it)
                                            << "if" << open_document
                                                << "$and" << open_array
                                                    << open_document
                                                        << "$eq" << open_array
                                                            << "$$value.stop" << false
                                                        << close_array
                                                    << close_document
                                                    << open_document
                                                        << "$lt" << open_array
                                                            << "$$bytes_sum"
                                                            << MAX_NUMBER_OF_BYTES
                                                        << close_array
                                                    << close_document
                                                << close_array
                                            << close_document
                                            << "then" << open_document
                                                << PROJECTED_TOTAL_BYTES << "$$bytes_sum"
                                                << PROJECTED_EXTRACTED_BYTES << "$$bytes_sum"
                                                << PROJECTED_TOTAL_COUNT_KEY << "$$count_sum"
                                                << PROJECTED_EXTRACTED_COUNT_KEY << "$$count_sum"
                                                << "stop" << false
                                            << close_document
                                            << "else" << open_document
                                                << PROJECTED_TOTAL_BYTES << "$$bytes_sum"
                                                << PROJECTED_EXTRACTED_BYTES << "$$value." + PROJECTED_EXTRACTED_BYTES
                                                << PROJECTED_TOTAL_COUNT_KEY << "$$count_sum"
                                                << PROJECTED_EXTRACTED_COUNT_KEY << "$$value." + PROJECTED_EXTRACTED_COUNT_KEY
                                                << "stop" << true
                                            << close_document

                                        << close_document

                                    << close_document
                                << close_document

                            << close_document

                        << close_document

                    << close_document

                << close_document
                << "in" << open_document
                    << PROJECTED_TOTAL_BYTES << open_document
                        << "$toLong" << "$$res_doc." + PROJECTED_TOTAL_BYTES
                    << close_document
                    << PROJECTED_EXTRACTED_BYTES << open_document
                        << "$toLong" << "$$res_doc." + PROJECTED_EXTRACTED_BYTES
                    << close_document
                    << PROJECTED_TOTAL_COUNT_KEY << open_document
                        << "$toInt" << "$$res_doc." + PROJECTED_TOTAL_COUNT_KEY
                    << close_document
                    << PROJECTED_EXTRACTED_COUNT_KEY << open_document
                        << "$toInt" << "$$res_doc." + PROJECTED_EXTRACTED_COUNT_KEY
                    << close_document
                << close_document
            << close_document

        << close_document
    << finalize;
}

mongocxx::pipeline buildMatchingActivityStatisticsPipeline(
        const std::string& SWIPE_PIPELINE_TYPE,
        const long& currentTimestampDay,
        const long& finalDayAlreadyGenerated
        ) {

    mongocxx::pipeline pipeline;

    pipeline.project(
        document{}
            << individual_match_statistics_keys::STATUS_ARRAY << 1
            << individual_match_statistics_keys::SENT_TIMESTAMP << 1
            << individual_match_statistics_keys::DAY_TIMESTAMP << 1
            << individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT << 1
            << match_algorithm_results_keys::ACCOUNT_TYPE << "$" + individual_match_statistics_keys::MATCHED_USER_ACCOUNT_DOCUMENT + "." + user_account_keys::ACCOUNT_TYPE
        << finalize
    );

    //add a field for the timestamp in days and for the type of the match
    pipeline.add_fields(
        document{}
            << SWIPE_PIPELINE_TYPE << open_document
                << "$reduce" << open_document
                    << "input" << "$" + individual_match_statistics_keys::STATUS_ARRAY
                    << "initialValue" << Stored_Type_Incomplete
                    << "in" << open_document

                        << "$cond" << open_document
                            << "if" << open_document
                                << "$eq" << open_array
                                    << "$$value"
                                    << Stored_Type_Incomplete
                                << close_array
                            << close_document

                            //if swipe type is Stored_Type_Incomplete then this is the first swipe
                            << "then" << open_document

                                << "$switch" << open_document

                                    << "branches" << open_array

                                        //if swipe type yes
                                        << open_document
                                            << "case" << open_document
                                                << "$eq" << open_array
                                                    << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                    << individual_match_statistics_keys::status_array::status_enum::YES
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_Yes_Incomplete
                                        << close_document

                                        //if swipe type no
                                        << open_document
                                            << "case" << open_document
                                                << "$eq" << open_array
                                                    << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                    << individual_match_statistics_keys::status_array::status_enum::NO
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_No
                                        << close_document

                                        //if swipe type block or report
                                        << open_document
                                            << "case" << open_document
                                                << "$or" << open_array
                                                    << "$eq" << open_array
                                                        << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                        << individual_match_statistics_keys::status_array::status_enum::BLOCK
                                                    << close_array
                                                    << "$eq" << open_array
                                                        << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                        << individual_match_statistics_keys::status_array::status_enum::REPORT
                                                    << close_array
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_Block_And_Report
                                        << close_document

                                    << close_array

                                    << "default" << Stored_Type_Incomplete
                                << close_document

                            << close_document

                            //if swipe type is NOT Stored_Type_Incomplete then this is the second swipe
                            << "else" << open_document

                                << "$switch" << open_document

                                    << "branches" << open_array

                                        //if swipe type yes
                                        << open_document
                                            << "case" << open_document
                                                << "$eq" << open_array
                                                    << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                    << individual_match_statistics_keys::status_array::status_enum::YES
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_Yes_Yes
                                        << close_document

                                        //if swipe type no
                                        << open_document
                                            << "case" << open_document
                                                << "$eq" << open_array
                                                    << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                    << individual_match_statistics_keys::status_array::status_enum::NO
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_Yes_No
                                        << close_document

                                        //if swipe type block or report
                                        << open_document
                                            << "case" << open_document
                                                << "$or" << open_array
                                                    << "$eq" << open_array
                                                        << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                        << individual_match_statistics_keys::status_array::status_enum::BLOCK
                                                    << close_array
                                                    << "$eq" << open_array
                                                        << "$$this." + individual_match_statistics_keys::status_array::STATUS
                                                        << individual_match_statistics_keys::status_array::status_enum::REPORT
                                                    << close_array
                                                << close_array
                                            << close_document
                                            << "then" << Stored_Type_Yes_Block_And_Report
                                        << close_document

                                    << close_array

                                    << "default" << Stored_Type_Yes_Incomplete
                                << close_document

                            << close_document
                        << close_document

                    << close_document
                << close_document
            << close_document
        << finalize
    );

    //exclude all days that are already stored
    pipeline.match(
        document{}
            << individual_match_statistics_keys::DAY_TIMESTAMP << open_document
                << "$gt" << finalDayAlreadyGenerated
            << close_document
            << individual_match_statistics_keys::DAY_TIMESTAMP << open_document
                << "$lt" << currentTimestampDay
            << close_document
        << finalize
    );

    pipeline.unwind("$" + individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT + "." + user_account_keys::accounts_list::ACTIVITY_STATISTICS);

    static const std::string COMPOUND_VAlUE_FOR_GROUPING = "comp_val";

    //create a field to group by, each day will be formatted as dayTimestamp_categoryType_categoryIndex
    pipeline.add_fields(
        document{}
            << COMPOUND_VAlUE_FOR_GROUPING << open_document
                << "$concat" << open_array
                    << open_document
                        << "$toString" << "$" + individual_match_statistics_keys::DAY_TIMESTAMP
                    << close_document
                    << "_"
                    << open_document
                        << "$toString" << "$" + match_algorithm_results_keys::ACCOUNT_TYPE
                    << close_document
                    << "_"
                    << open_document
                        << "$toString" << "$" + individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT + "." + user_account_keys::accounts_list::ACTIVITY_STATISTICS + "." + user_account_keys::categories::TYPE
                    << close_document
                    << "_"
                    << open_document
                        << "$toString" << "$" + individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT + "." + user_account_keys::accounts_list::ACTIVITY_STATISTICS + "." + user_account_keys::categories::INDEX_VALUE
                    << close_document
                << close_array
            << close_document
        << finalize
    );

    auto generateSwipeTypeCheck = [&](StoredType stored_type)->bsoncxx::document::value {
        return document{}
            << "$sum" << open_document
                << "$cond" << open_document

                    << "if" << open_document
                        << "$eq" << open_array
                            << "$" + SWIPE_PIPELINE_TYPE
                            << stored_type
                        << close_array
                    << close_document

                    << "then" << bsoncxx::types::b_int64{1L}

                    << "else" << bsoncxx::types::b_int64{0L}

                << close_document
            << close_document
        << finalize;
    };

    pipeline.group(
        document{}
            << "_id" << "$" + COMPOUND_VAlUE_FOR_GROUPING

            //these will all be the same because they were combined to form COMPOUND_VAlUE_FOR_GROUPING
            << match_algorithm_results_keys::GENERATED_FOR_DAY << open_document
                << "$first" << "$" + individual_match_statistics_keys::DAY_TIMESTAMP
            << close_document
            << match_algorithm_results_keys::ACCOUNT_TYPE << open_document
                << "$first" << "$" + match_algorithm_results_keys::ACCOUNT_TYPE
            << close_document
            << match_algorithm_results_keys::CATEGORIES_TYPE << open_document
                << "$first" << "$" + individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT + "." + user_account_keys::accounts_list::ACTIVITY_STATISTICS + "." + user_account_keys::categories::TYPE
            << close_document
            << match_algorithm_results_keys::CATEGORIES_VALUE << open_document
                << "$first" << "$" + individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT + "." + user_account_keys::accounts_list::ACTIVITY_STATISTICS + "." + user_account_keys::categories::INDEX_VALUE
            << close_document

            << match_algorithm_results_keys::NUM_YES_YES << generateSwipeTypeCheck(Stored_Type_Yes_Yes)
            << match_algorithm_results_keys::NUM_YES_NO << generateSwipeTypeCheck(Stored_Type_Yes_No)
            << match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT << generateSwipeTypeCheck(Stored_Type_Yes_Block_And_Report)
            << match_algorithm_results_keys::NUM_YES_INCOMPLETE << generateSwipeTypeCheck(Stored_Type_Yes_Incomplete)
            << match_algorithm_results_keys::NUM_NO << generateSwipeTypeCheck(Stored_Type_No)
            << match_algorithm_results_keys::NUM_BLOCK_AND_REPORT << generateSwipeTypeCheck(Stored_Type_Block_And_Report)
            << match_algorithm_results_keys::NUM_INCOMPLETE << generateSwipeTypeCheck(Stored_Type_Incomplete)
        << finalize
    );

    //NOTE: while the "_id" could be used as the oid for the document there is no value in it
    // this is because it is essentially identical to the compound index already created, however
    // it will not work for individual fields. So simply allowing mongoDB to generate a standard OID.
    pipeline.project(
        document{}
            << "_id" << 0
        << finalize
    );

    return pipeline;
}

mongocxx::pipeline buildPipelineForInsertingDocument(
        const bsoncxx::document::view& message_to_insert
        ) {

    mongocxx::pipeline pipeline;

    //The document is inserted this way because the 'closer' the timestamp is generated to the insertion
    // the more likely that the documents will end up ordered. For example if the timestamp is generated
    // at the beginning of client_message_to_server then passed in here, 2 threads could start near the
    // same time and the later timestamp could be inserted first.
    //The reason this is important is that the chat change stream will receive messages in the order they
    // were inserted. If the messages are out of order with their respective timestamps then the client
    // can end up with a 'gap' in the timestamps. This can cause their chat_room_last_time_updated to be
    // off and for messages to be missed.
    pipeline.replace_root(
        document{}
            << "newRoot" << open_document

                << "$cond" << open_document

                    //If timestamp field does not exist (this should be the normal case), then the document
                    // does not exist. If message type is a placeholder, update it.
                    << "if" << open_document
                        << "$eq" << open_array
                            << "$" + chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_undefined{}
                        << close_array
                    << close_document

                    //then create document
                    << "then" << message_to_insert

                    //if field does exist, do nothing
                    << "else" << "$$ROOT"

                << close_document

            << close_document
        << finalize
        );

    return pipeline;
}

mongocxx::pipeline buildPipelineForCancelingEvent() {
    mongocxx::pipeline pipe;

    //Clear categories timeframes array. This is just to guarantee that the device can interpret the event is expired.
    pipe.add_fields(
        document{}
            << user_account_keys::MATCHING_ACTIVATED << false
            << user_account_keys::EVENT_EXPIRATION_TIME << general_values::event_expiration_time_values::EVENT_CANCELED
            << user_account_keys::CATEGORIES << open_document
                << "$map" << open_document
                    << "input" << "$" + user_account_keys::CATEGORIES
                    << "in" << open_document
                        << "$mergeObjects" << open_array
                            << "$$this"
                            << open_document
                                << user_account_keys::categories::TIMEFRAMES << open_array
                                << close_array
                            << close_document
                        << close_array
                    << close_document
                << close_document
            << close_document
        << finalize
    );

    return pipe;
}