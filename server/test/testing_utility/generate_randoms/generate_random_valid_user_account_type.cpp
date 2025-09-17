//
// Created by jeremiah on 3/22/23.
//

#include "generate_randoms.h"

UserAccountType generateRandomValidUserAccountType() {
    const int case_num = rand() % 3;
    switch(case_num) {
        case 0:
            return UserAccountType::ADMIN_GENERATED_EVENT_TYPE;
        case 1:
            return UserAccountType::USER_GENERATED_EVENT_TYPE;
        default:
            return UserAccountType::USER_ACCOUNT_TYPE;
    }
}