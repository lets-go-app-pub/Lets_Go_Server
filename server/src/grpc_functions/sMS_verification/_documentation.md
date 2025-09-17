
## Create New Account Flow

The smsVerification() function is always called after the login function. It will rely on a pending account existing 
 which the login function will create. Also, the input and output to the previously run login function will need to
 be taken into account.

See login_function/_documentation for more info on this subject as well as more info about the collections and the 
 flow of logging in.

## Sms verification input

The input message is SMSVerificationResponse.

The first few fields are mandatory, including.
 * account_type; enum
 * phone_number_or_account_id; Either the phone number or the account id depending on what was passed to the
  login function to create the pending account.
 * verification_code; The verification code as entered by the user, will be compared to pending account verification 
  code.
 * lets_go_version; client version
 * installation_id; The installation id that was passed to the login function to create the pending account.

The remaining fields are only used for a specific case. This is when the login function returns
 birthday_not_needed == false. This return means that the installation id does not exist inside the user account
 INSTALLATION_IDS array and must be added. One of two sets of information should be passed in for this case.
 1) installation_id_added_command == UPDATE_ACCOUNT; The means that the current account will have the installation id
  added to it. The user is expected to enter the birthday stored inside the user account for the function to complete 
  successfully. The fields for birth_year, birth_month and birth_day_of_month are expected to be filled with the user
  entered birthday data.
 2) installation_id_added_command == CREATE_NEW_ACCOUNT; This means that **THE OLD USER ACCOUNT WILL BE DELETED AND A
  FRESH USER ACCOUNT WILL BE CREATED**. This option does not require any birthday info. It is however very important
  that **THE USER IS AWARE THAT THIS WILL HAPPEN** and that it is asked twice (a popup of 'are you sure' for example). 

NOTE: The reason the installation ids work so oddly is because of a specific situation. Say
 1) Person_A creates an account with phoneNumber_1.
 2) Person_A for some reason changes their phone number to phoneNumber_2 but does not update their account.
 3) Person_B gets phoneNumber_1 as their number and logs in.

This would allow Person_B to see all Person_As' information inside the account. Instead, a birthday is required to make 
 sure that Person_B is the account creator. Also, this will allow Person_B to just create a new account for themselves.
 The downside is that it has the potential for people to accidentally delete their account if they attempt to sign in
 from a new device. However, double-checking with the user should greatly minimize this issue.

## Sms verification output

The input message is SMSVerificationResponse.

There is only one field inside the response, return_status, this is a list of the possible values.
 * VALUE_NOT_SET; This is the default value and should only be set if a failure in gRPC occurred.
 * SUCCESS; Successfully created account or updated existing account.
 * OUTDATED_VERSION; lets_go_version was outdated.
 * VERIFICATION_CODE_EXPIRED; The verification code passed has expired and can no longer be used.
 * INVALID_PHONE_NUMBER_OR_ACCOUNT_ID; phone_number_or_account_id was invalid.
 * INVALID_VERIFICATION_CODE; verification_code was either invalid (size not VERIFICATION_CODE_NUMBER_OF_DIGITS) or
  did not match stored code.
 * INVALID_INSTALLATION_ID; installation_id was not a valid uuid.
 * PENDING_ACCOUNT_NOT_FOUND; Pending account (searched for by phone number & installation Id) was not found.
 * INCORRECT_BIRTHDAY; When an installation id was passed and installation_id_added_command == UPDATE_ACCOUNT. The
  birthday passed did not match the birthday stored in the user account.
 * INVALID_UPDATE_ACCOUNT_METHOD_PASSED; installation_id_added_command was an invalid value when attempting to add an
  installation id to INSTALLATION_IDS.
 * UNKNOWN; This means the function didn't set it for some reason.
 * DATABASE_DOWN; Error with database on server.
 * LG_ERROR; error

---

## Cracking verification codes

There is the potential for a brute force attack where someone writes a script that simply ignores the device and sends
 every possible verification code to the server. It is a six digit number (it is a number for ease of access to all 
 languages). This means there are only a million possibilities.

In order to prevent this each account has fields inside the INFO_STORED_AFTER_DELETION_COLLECTION dedicated to limiting
 the number of attempts on sms verification.

The way it works is that each account has MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS inside 
 TIME_BETWEEN_VERIFICATION_ATTEMPTS time. If this limit is reached, no more verification attempts can be made on this 
 account until the time has expired. 

Stats on max of 20 verification codes guessed per hour (the hour resets on the hour for ease of implementation).
 * Because they have the ability to guess 40 in 2 1 hour windows. This means 40/1000000 possibilities every 2 hours, so
  a 0.99996 chance of NOT guessing the code.
 * So after 5000 hours (~208 days) the chance of guessing the code are 0.99996^2500=0.9048 so 1-0.9048 = 9.52%.

Other ways to do this might include
 1) Give each verification code its own limited number of attempts.
 2) Reset the codes based on attempts, not just at the top of the hour.
 3) Combinations, for example give each verification code 5 attempts, then give the account a total of 20 attempts 
  before lockout.

Security for our users is important. However, while we want to make sure that these restrictions are detrimental to bad
 actors, they should have minimal effect on the actual users.
