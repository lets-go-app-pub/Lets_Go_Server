
## Create New Account Flow

If a user connects with a phone number or account id the process of creating a new account will begin.
1) It will insert an account to INFO_STORED_AFTER_DELETION_COLLECTION or update it if it already exists. If sms sending
 is not on cooldown it will upsert an account to PENDING_ACCOUNT_COLLECTION.
2) The device will be expected to then send the verification code received from the sms back to the smsVerification()
 function. This will remove the account from PENDING_ACCOUNT_COLLECTION. It will then create a new account for the
 user inside USER_ACCOUNT_COLLECTION.
3) When the user account is first created the user_account_keys::STATUS is set to 
 STATUS_REQUIRES_MORE_INFO. Several fields will be required to be set by the device before the 
 status is set to STATUS_ACTIVE. These include birthday, email, gender, name, pictures and 
 activities (everything not designated as 'post_login_info').
4) After all values have been set the status is set from STATUS_REQUIRES_MORE_INFO to STATUS_ACTIVE by calling 
 findNeededVerificationInfo(). This will check that all required information has been set and allow the account to
 participate in chat and matching (as well as a few other features).

---

## Login Input

The protobuf message used for login input is LoginRequest.

Certain information is always required by the login function including.
 * lets_go_version
 * account_type
 * installation_id
 * if (account_type == PHONE_ACCOUNT) phone_number is mandatory
 * if (account_type == FACEBOOK || GOOGLE) account_id is mandatory
 * device_name should always be set for statistics purposes (will not fail without it)
 * api_number should always be set for statistics purposes (will not fail without it)

If connecting with no information inside device database the mandatory information above is almost enough.
 * if (account_type == FACEBOOK || GOOGLE) account_id AND phone_number are mandatory

If connecting for a login in order to either refresh token or when info is already stored more info is required.
 * birthday_timestamp; Can send in -1 if it does not exist on device.
 * email_timestamp; Can send in -1 if it does not exist on device.
 * gender_timestamp; Can send in -1 if it does not exist on device.
 * name_timestamp; Can send in -1 if it does not exist on device.
 * categories_timestamp; Can send in -1 if it does not exist on device.
 * post_login_info_timestamp; Can send in -1 if it does not exist on device.
 * icon_timestamps; The user is expected to send all icon timestamps ordered by index of the last time the client 
 updated each one.

---

## Login Output

Possible errors include (ordered by return_status).
 * OUTDATED_VERSION; invalid lets_go_version passed
 * INVALID_ACCOUNT_TYPE; invalid account_type
 * INVALID_INSTALLATION_ID; invalid installation_id
 * INVALID_PHONE_NUMBER_OR_ACCOUNT_ID;
     1) account_type==PHONE_ACCOUNT and invalid phone number
     2) account_type==(FACEBOOK || GOOGLE) and invalid account id
 * REQUIRES_PHONE_NUMBER_TO_CREATE_ACCOUNT; account does not exist && (account_type==(FACEBOOK || GOOGLE) && invalid phone number
 * ACCOUNT_CLOSED; suspend or banned
     1) Account suspended, set_access_status will be set to SUSPENDED, time_out_duration_remaining will be set to the 
      time remaining on suspension in ms, time out message will be set to reason user was suspended.
     2) Account banned, set_access_status will be set to BANNED, time out message will be set to reason user was
      banned.
 * SMS_ON_COOL_DOWN & REQUIRES_AUTHENTICATION; These two returns will happen in the below cases depending if the sms
  is on cooldown or not. SMS_ON_COOL_DOWN will always be set along with the field sms_cool_down.
     1) Account does not exist.
     2) New installation id passed (does not exist inside user_account_keys::INSTALLATION_IDS). This will also set 
        birthday_not_needed to false and access_status to ACCESS_GRANTED (can look at code for reason why 
        birthday_not_needed is needed).
     3) New account id passed (does not exist inside user_account_keys::ACCOUNT_ID_LIST). This will also set
        access_status to ACCESS_GRANTED.
     4) Account requires verification every TIME_BETWEEN_ACCOUNT_VERIFICATION time. This will also set
        access_status to ACCESS_GRANTED.

If login was successful (return_status==LOGGED_IN) it will always return certain fields.
 * return_status; should be set to LOGGED_IN
 * access_status; set to NEEDS_MORE_INFO if account status == STATUS_REQUIRES_MORE_INFO otherwise set to ACCESS_GRANTED
 * account_oid; user account oid
 * login_token; login token (a UUID)
 * birthday_not_needed; set to true if it made it here
 * login_values_to_return_to_client
     1) server_categories; List of all possible categories from server.
     2) server_activities; List of all possible activities from server.
     3) icons_index; List of all icons that require an update (it returns the icon index, could be empty).
     4) global_constant_values; Various global values used by the client.
 * server_timestamp; the time generated by the server (used to set the time on client)
 * pre_login_timestamps; actual timestamps update to current values by server
 * pictures_timestamps; array of picture timestamps by index; -1 will be used for empty pictures
 * post_login_timestamp; the time generated by the server
 * algorithm_search_options; search options
 * phone_number; phone number
 * blocked_accounts; list of user account oids this user has blocked

If login was successful (return_status==LOGGED_IN) these fields can be optionally sent back.
 * birthday_info; set if request.birthday_timestamp was out of date
 * email_info; set if request.email_timestamp was out of date
 * gender; set if request.gender_timestamp was out of date
 * name; set if request.name_timestamp was out of date
 * categories_array; set if request.categories_timestamp was out of date
 * post_login_info; set if request.post_login_info_timestamp was out of date

---

## Collections

### User Accounts Collection

This collection named USER_ACCOUNTS_COLLECTION_NAME stores the full user account (not including statistics). It is used
 for storing user information as well as running the matching algorithm. There are three indexes on the collection.
 1) user_account_keys::PHONE_NUMBER; This index is unique and used by a variety of functions to find the user account.
 2) user_account_keys::ACCOUNT_ID_LIST; This index is unique and used by the login function to find the user account if
  logging in using account_type==(FACEBOOK || GOOGLE).
 3) The algorithm index named ALGORITHM_INDEX_NAME. Specifics can be seen for this index inside 
  find_matches/_documentation.md.

### Info Stored After Deletion Collection

This collection named INFO_STORED_AFTER_DELETION_COLLECTION_NAME stores info by phone number if the user account is
 deleted. It is created with the pending account when login is first called and then will be accessed if no use account
 exists for the phone number passed when calling loginFunction(). The purpose of it is to avoid unnecessary fringe 
 cases such as a user quickly creating a user account then deleting their account to get more swipes or to spam sms. 

There is only one index for this collection.
 1) info_stored_after_deletion_keys::PHONE_NUMBER; This index is unique and used to access the document by a variety 
 of functions.

### Pending Account Collection

This collection is used for account verification. If a user calls loginFunction() and sms verification is required a
 pending account will be created. Cases for how this could occur are listed above under the header 'Login Output' when
 REQUIRES_AUTHENTICATION is returned.

This collection has four indexes.
 1) pending_account_keys::INDEXING; This index is unique and used to access the document by smsVerification() and
  loginFunction().
 2) pending_account_keys::ID; This index is unique and used to access the document by smsVerification() and
  loginFunction().
 3) pending_account_keys::PHONE_NUMBER; This index is unique and used to access the document by loginFunction().
 4) pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT; This is a mongoDB TTL index. It will be set to the time the
  document was created and when general_values::TIME_UNTIL_PENDING_ACCOUNT_REMOVED time has passed, mongoDB will 
  automatically remove the document.

The choice of using indexes this way has some pros and cons associated with it. The reason that three unique indexes 
 are used instead of a unique compound index is simply that the pending account documents are expected to be unique by
 all the fields that are unique above. If a compound index is used then multiple pending accounts can exist for the
 same phone number, installation id or indexing field. This could allow multiple phone numbers (or any of the other
 uniquely indexed fields) to be pending at the same time and so could result in multiple accounts attempted to be 
 created with the same info if called simultaneously.

When loginFunction() generates a pending account document it will run a query for INDEXING || ID || PHONE_NUMBER and
 upsert any hits. The downside of using the unique indexes this way is that there is the possibility of different
 combinations of fields. 

As an example say
 1) A pending account is created with phone_number_1 and installation_id_1. 
 2) Another pending account is created with phone_number_2 and installation_id_2.
 3) loginFunction() is called with phone_number_1 and installation_id_2.

This will cause the upsert to fail because the unique index already exists. In order to mitigate this problem if a
 duplicate is found then all pending accounts with matching pending_account_keys::INDEXING and 
 pending_account_keys::PHONE_NUMBER will be deleted (pending_account_keys::ID which is the installation id should be
 unique anyway). Then the upsert will be run again.

---

## Spamming

There is a system in place to stop users from spamming sms verification messages for different phone numbers. It
 uses the PRE_LOGIN_CHECKERS_COLLECTION and counts the number of sms messages sent by a certain installation ID. If they
 send NUMBER_SMS_VERIFICATION_MESSAGES_SENT messages in TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID time,
 they will be blocked from sending any sms verification messages until the timer resets.

There are some problems with this approach.
 1) If the user wants to (and realises it) they can simply uninstall and reinstall and get a new installation ID to 
  continue spamming.
 2) This does not stop people writing scripts as they can just generate a random UUID (installation ID) each time.

A more complete approach may also take into account IP address, however this also comes with several problems.
 1) Multiple users can exist on the same IP address, for example at a coffee shop or a college campus.
 2) If a user has the ability to spoof a device ID, they have the ability to change their IP address. It could make the
  process more complex though which may be worthwhile later.

The underlying issue is that before the user has logged in we have no guarantee of validity on any of their information. So
 everything is spoof-able. While the restriction will handle simple cases, the longer term solution is simply to not
 give any benefit to people writing bots or scripts to spam these things.
