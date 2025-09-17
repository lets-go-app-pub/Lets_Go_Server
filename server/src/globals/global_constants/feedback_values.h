//
// Created by jeremiah on 11/18/21.
//

#pragma once

namespace feedback_values {
    inline const int NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT = 10; //will request this number of feedback after the most recently viewed feedback & the most recently viewed feedback plus this number minus 1
    inline const int NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE = 5; //will request this number of feedback AT MOST when update is called (also adds another element to response to 'cap' the vector)

    inline const int MAX_NUMBER_OF_TIMES_CAN_SPAM_FEEDBACK = 10; //if an admin reports this account this number of times for spam, their feedback will no longer be looked at
}