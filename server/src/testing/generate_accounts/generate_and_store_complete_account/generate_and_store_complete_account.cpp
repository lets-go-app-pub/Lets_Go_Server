//
// Created by jeremiah on 3/20/21.
//

#include <LoginFunction.grpc.pb.h>
#include <SMSVerification.grpc.pb.h>
#include <SetFields.grpc.pb.h>
#include <LoginSupportFunctions.grpc.pb.h>
#include <utility_testing_functions.h>
#include <login_support_functions.h>
#include <utility_general_functions.h>

#include "generate_and_store_complete_account.h"


#include "set_fields_functions.h"
#include "sMS_verification.h"
#include "login_function.h"
#include "general_values.h"

std::string generateAndStoreCompleteAccount(
        const std::string& phoneNumber, const std::string& email, int birthYear, int birthMonth,
        int birthDayOfMonth, const std::string& firstName, const std::string& gender,
        const std::vector<std::tuple<const int, const std::string, const std::string>>& pictureAndIndexVector,
        const std::vector<ActivityStruct>& categories, const double& longitude,
        const double& latitude, int minAgeRange, int maxAgeRange,
        const std::vector<std::string>& genderRange, int maxDistance,
        const std::string& city, const std::string& bio,
        const std::string& user_account_oid) {

    const std::string installationID = generateUUID();
    const int letsGoVersion = general_values::MINIMUM_ACCEPTED_ANDROID_VERSION + 1;

    loginfunction::LoginRequest loginRequest;
    loginfunction::LoginResponse loginResponse;

    loginRequest.set_phone_number(phoneNumber);
    loginRequest.set_account_id(user_account_oid);
    loginRequest.set_installation_id(installationID);
    loginRequest.set_lets_go_version(letsGoVersion);
    loginRequest.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    const std::string verificationCode = loginFunctionMongoDb(&loginRequest, &loginResponse, "addr:port");

    if (loginResponse.return_status() != LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION) {
        std::cout << "FAIL: Account creation failed login returned "
                     + convertLoginFunctionStatusToString(loginResponse.return_status()) + " expected "
                     + convertLoginFunctionStatusToString(LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION)
                     + "\nphoneNumber: " + phoneNumber + '\n';
        return "";
    }

    sms_verification::SMSVerificationRequest smsVeriRequest;
    sms_verification::SMSVerificationResponse smsVeriResponse;

    smsVeriRequest.set_account_type(AccountLoginType::PHONE_ACCOUNT);
    smsVeriRequest.set_phone_number_or_account_id(phoneNumber);
    smsVeriRequest.set_verification_code(verificationCode);
    smsVeriRequest.set_lets_go_version(letsGoVersion);
    smsVeriRequest.set_installation_id(installationID);

    //I think this 'hits' duplicate phone numbers sometimes and errors?
    // happens inside the line request->update_account_method() != sms_verification::SMSVerificationRequest_UpdateAccount_UPDATE_ACCOUNT
    // ? the probabilities of that should be 10^10? or 1/10,000,000,000, and it happened twice in 100,000 random accounts,
    // so it might be something else? however, essentially an installation ID already existed inside the account which should
    // only be possible if the sms verification already ran
    sMSVerification(&smsVeriRequest, &smsVeriResponse);

    if (smsVeriResponse.return_status() !=
        sms_verification::SMSVerificationResponse::Status::SMSVerificationResponse_Status_SUCCESS) {
        std::cout << "FAIL: Account creation failed smsVerification returned "
                     + convertSmsVerificationStatusToString(smsVeriResponse.return_status()) + " expected "
                     + convertSmsVerificationStatusToString(
                sms_verification::SMSVerificationResponse::Status::SMSVerificationResponse_Status_SUCCESS) + '\n';
        return "";
    }

    loginFunctionMongoDb(&loginRequest, &loginResponse, "addr:port");

    if (loginResponse.return_status() != LoginValuesToReturnToClient::LOGGED_IN) {
        std::cout << "FAIL: Account creation failed login returned "
                     + convertLoginFunctionStatusToString(loginResponse.return_status()) + " expected "
                     + convertLoginFunctionStatusToString(LoginValuesToReturnToClient::LOGGED_IN) + '\n';
        return "";
    }

    const std::string loginToken = loginResponse.login_token();
    const std::string loginAccountOID = loginResponse.account_oid();

    auto setLoginInfo = [&](LoginToServerBasicInfo* loginInfo) {
        loginInfo->set_current_account_id(loginAccountOID);
        loginInfo->set_logged_in_token(loginToken);
        loginInfo->set_lets_go_version(letsGoVersion);
        loginInfo->set_installation_id(installationID);
    };

    //picture, categories
    setfields::SetEmailRequest emailRequest;
    setfields::SetFieldResponse emailResponse;

    setLoginInfo(emailRequest.mutable_login_info());

    emailRequest.set_set_email(email);

    setEmail(&emailRequest, &emailResponse);

    if (emailResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed set email returned \n"
                     + convertReturnStatusToString(emailResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nemail: " + email + '\n';
        return "";
    }

    setfields::SetBirthdayRequest birthdayRequest;
    setfields::SetBirthdayResponse birthdayResponse;

    setLoginInfo(birthdayRequest.mutable_login_info());

    birthdayRequest.set_birth_year(birthYear);
    birthdayRequest.set_birth_month(birthMonth);
    birthdayRequest.set_birth_day_of_month(birthDayOfMonth);

    setBirthday(&birthdayRequest, &birthdayResponse);

    if (birthdayResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed set birthday returned "
                     + convertReturnStatusToString(birthdayResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nbirthYear: " + std::to_string(birthYear)
                     + "\nbirthMonth: " + std::to_string(birthMonth)
                     + "\nbirthDayOfMonth: " + std::to_string(birthDayOfMonth)
                     + "\nbirthdayRequest\n" + birthdayRequest.DebugString() + '\n';
        return "";
    }

    setfields::SetStringRequest nameRequest;
    setfields::SetFieldResponse nameResponse;

    setLoginInfo(nameRequest.mutable_login_info());

    nameRequest.set_set_string(firstName);

    setFirstName(&nameRequest, &nameResponse);

    if (nameResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed set first name returned "
                     + convertReturnStatusToString(nameResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nfirstName: " + firstName + '\n';
        return "";
    }

    setfields::SetStringRequest stringRequest;
    setfields::SetFieldResponse genderResponse;

    setLoginInfo(stringRequest.mutable_login_info());

    stringRequest.set_set_string(gender);

    setGender(&stringRequest, &genderResponse);

    if (genderResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed set gender returned "
                     + convertReturnStatusToString(genderResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\ngender: " + gender + '\n';
        return "";
    }

    for (size_t i = 0; i < pictureAndIndexVector.size(); i++) {

        setfields::SetPictureRequest pictureRequest;
        setfields::SetFieldResponse pictureResponse;

        setLoginInfo(pictureRequest.mutable_login_info());

        const auto&[index, picture, thumbnail] = pictureAndIndexVector[i];

        pictureRequest.set_file_in_bytes(picture);
        pictureRequest.set_file_size((int) picture.size());
        pictureRequest.set_thumbnail_in_bytes(thumbnail);
        pictureRequest.set_thumbnail_size((int) thumbnail.size());
        pictureRequest.set_picture_array_index(index);

        setPicture(&pictureRequest, &pictureResponse);

        if (pictureResponse.return_status() != ReturnStatus::SUCCESS) {
            std::cout << "FAIL: Account creation failed set picture index " + std::to_string(i) + " returned\n"
                         + convertReturnStatusToString(pictureResponse.return_status()) + " expected "
                         + convertReturnStatusToString(ReturnStatus::SUCCESS)
                         + "\nThumbnail File Size: " + std::to_string(std::get<2>(pictureAndIndexVector[i]).size())
                         + "\nPicture File Size: " + std::to_string(std::get<1>(pictureAndIndexVector[i]).size())
                         + "\nPicture Array index: " + std::to_string(std::get<0>(pictureAndIndexVector[i])) + '\n';

            return "";
        }
    }

    setfields::SetCategoriesRequest categoriesRequest;
    setfields::SetFieldResponse categoriesResponse;

    setLoginInfo(categoriesRequest.mutable_login_info());

    for (const auto& categoryItem : categories) {
        auto category = categoriesRequest.add_category();

        for (size_t j = 0; j < categoryItem.timeFrames.size(); j++) {
            auto timeframe = category->add_time_frame_array();

            if (j == 0 && categoryItem.timeFrames[j].startStopValue == -1) {
                timeframe->set_start_time_frame(-1);
                timeframe->set_stop_time_frame(categoryItem.timeFrames[j].time.count());
            } else {
                timeframe->set_start_time_frame(categoryItem.timeFrames[j].time.count());
                timeframe->set_stop_time_frame(categoryItem.timeFrames[j + 1].time.count());
                j++;
            }
        }

        category->set_activity_index(categoryItem.activityIndex);
    }

    setCategories(&categoriesRequest, &categoriesResponse);

    if (categoriesResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed set categories returned "
                     + convertReturnStatusToString(categoriesResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS);
        return "";
    }

    loginsupport::NeededVeriInfoRequest needVeriRequest;
    loginsupport::NeededVeriInfoResponse needVeriResponse;

    setLoginInfo(needVeriRequest.mutable_login_info());

    needVeriRequest.set_client_longitude(longitude);
    needVeriRequest.set_client_latitude(latitude);

    findNeededVerificationInfo(&needVeriRequest, &needVeriResponse);

    if (needVeriResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Account creation failed find needed verification info returned "
                     + convertReturnStatusToString(needVeriResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nLongitude: " + std::to_string(longitude)
                     + "\nLatitude: " + std::to_string(latitude) + '\n';

        return "";
    }

    if (needVeriResponse.access_status() != AccessStatus::ACCESS_GRANTED) {
        std::cout << "FAIL: Account creation failed find needed verification info returned "
                     + convertAccessStatusToString(needVeriResponse.access_status()) + " expected "
                     + convertAccessStatusToString(AccessStatus::ACCESS_GRANTED);

        return "";
    }

    setfields::SetAgeRangeRequest ageRangeRequest;
    setfields::SetFieldResponse ageRangeResponse;

    setLoginInfo(ageRangeRequest.mutable_login_info());

    ageRangeRequest.set_min_age(minAgeRange);
    ageRangeRequest.set_max_age(maxAgeRange);

    setAgeRange(&ageRangeRequest, &ageRangeResponse);

    if (ageRangeResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Set age range returned "
                     + convertReturnStatusToString(ageRangeResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nminAgeRange: " + std::to_string(minAgeRange)
                     + "\nmaxAgeRange: " + std::to_string(maxAgeRange) + '\n';
        return "";
    }

    setfields::SetGenderRangeRequest genderRangeRequest;
    setfields::SetFieldResponse genderRangeResponse;

    setLoginInfo(genderRangeRequest.mutable_login_info());

    for (const auto& genderItem : genderRange) {
        genderRangeRequest.add_gender_range(genderItem);
    }

    setGenderRange(&genderRangeRequest, &genderRangeResponse);

    if (genderRangeResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Set gender range returned "
                     + convertReturnStatusToString(genderRangeResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS) + '\n'
                     + "Gender Range {\n";

        for (const auto& genderItem : genderRange) {
            std::cout << "  gender: " << genderItem << '\n';
        }

        std::cout << "}\n";
        return "";
    }

    //max distance
    setfields::SetMaxDistanceRequest maxDistanceRequest;
    setfields::SetFieldResponse maxDistanceResponse;

    setLoginInfo(maxDistanceRequest.mutable_login_info());

    maxDistanceRequest.set_max_distance(maxDistance);

    setMaxDistance(&maxDistanceRequest, &maxDistanceResponse);

    if (maxDistanceResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Set max distance returned "
                     + convertReturnStatusToString(maxDistanceResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nmaxDistance: " + std::to_string(maxDistance) + '\n';
        return "";
    }

    //set city
    setfields::SetStringRequest setCityRequest;
    setfields::SetFieldResponse setCityResponse;

    setLoginInfo(setCityRequest.mutable_login_info());

    setCityRequest.set_set_string(city);

    setUserCity(&setCityRequest, &setCityResponse);

    if (setCityResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Set city returned "
                     + convertReturnStatusToString(setCityResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\ncity: " + city + '\n';
        return "";
    }

    //set bio
    setfields::SetBioRequest setBioRequest;
    setfields::SetFieldResponse setBioResponse;

    setLoginInfo(setBioRequest.mutable_login_info());

    setBioRequest.set_set_string(bio);

    setUserBio(&setBioRequest, &setBioResponse);

    if (setBioResponse.return_status() != ReturnStatus::SUCCESS) {
        std::cout << "FAIL: Set city returned "
                     + convertReturnStatusToString(setBioResponse.return_status()) + " expected "
                     + convertReturnStatusToString(ReturnStatus::SUCCESS)
                     + "\nbio: " + bio + '\n';
        return "";
    }

    return loginAccountOID;
}