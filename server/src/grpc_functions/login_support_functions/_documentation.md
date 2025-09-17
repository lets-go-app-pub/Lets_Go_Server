## findNeededVerificationInfo()

This function will set up the account to participate in matches and chat. More info can be found on the flow of
creating an account inside login_function/_documentation.md.

### Required info

In order for this function to be called several fields are expected to have been set. These fields are
 * birthday info
 * email address
 * gender
 * first name
 * activities
 * pictures; This is not checked because zero pictures is valid, however it should be set by the client before 
  findNeededVerificationInfo() is called.

With the exception of pictures if any of the above fields are **not** set then the function will not update the account
to UserAccountStatus::STATUS_ACTIVE.

### Output

The protobuf message for the response is NeededVeriInfoResponse. It contains a return_status field which will be set
 if any errors occur. This output discusses when return_status==SUCCESS.

There are two major cases to take into consideration.

1) access_status == AccessStatus::ACCESS_GRANTED; This will be returned if the function was successful. It will also 
 fill out all pre_login_timestamps, picture_timestamps and any post_login_info that has been set (city and bio will be
 set to "~" which means empty).
2) access_status == AccessStatus::NEEDS_MORE_INFO; This will be returned if all required info (see 'Required Info'
 header above) has not yet been set. All picture_timestamps will be set, pre_login_timestamps will be set except the
 value(s) which are still required by the server, these will be set to -1.

---

## logoutFunction()

From the users' point of view logout is a straightforward action. However, login also performs some heavier tasks. 
 The major point is that logout will check for any inconsistencies and attempt to fix them somehow. The inconsistencies
 checked for are as follows.

 1) Age; The function will make sure the users' age is valid. If it is NOT valid it will set the user to a specific age
  and adjust the birth year to match.
 2) Pictures; The function will make sure that all pictures inside the user_account_keys::PICTURES have a matching
  picture inside USER_PICTURES_COLLECTION. If there is no matching picture then the array element will be removed.
 3) Chat Rooms; The function will make sure that all chat rooms listed inside user_account_keys::CHAT_ROOMS are also
  inside the respective chat room header as either ACCOUNT_STATE_IN_CHAT_ROOM or ACCOUNT_STATE_IS_ADMIN. If any 
  inconsistencies occur, the function will fix them by removing the user.
 4) Matches; The function will make sure that all matches are also properly set up as matching chat rooms inside the
  chat room header. Anything that is NOT a matching chat room inside the header will be removed from the user
  OTHER_ACCOUNTS_MATCHED_WITH array.
