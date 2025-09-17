## ageGenderStatistics()

This function will extract the number of genders at each age. 

For example if we have a few users
 * userA; gender Male, age 21
 * userB; gender Female, age 44
 * userC; gender Bisexual, age 78
 * userD; gender Asexual, age 21

This would send back three items.
 * age 21; 1 Male, 0 Females, 1 Other gender 
 * age 44; 0 Male, 1 Females, 0 Other gender 
 * age 78; 0 Male, 0 Females, 1 Other gender 

Under the hood the function will iterate the entire USER_ACCOUNTS_COLLECTION to collect the information. Because of 
 this each admin has a cooldown of COOL_DOWN_BETWEEN_REQUESTING_AGE_GENDER_STATISTICS between requests.

---

## matchingActivityStatistics()

This function will request the number of swipes in the given timeframe. 

For example say the user requests all swipes for the last 20 days (note that it cannot request matches for today, only
 previous days). This will search through the match history and return each activity or category that has had at least
 one swipe on it. This is returned inside the activity_statistics field of MatchingActivityStatisticsResponse.

There are filters that can also applied to the request. 'include_categories' will include swipes in
 which only categories and no activities were matched. The other options are all related to the user responses (calls to
 userMatchOptions()) that have taken place. For example 'yes_no' will occur when the first user swiped yes and then the 
 second user swiped no, 'incomplete' will be when the first user has not yet responded to the match.

Under the hood the way that this works involves two collections, MATCH_ALGORITHM_RESULTS_COLLECTION and
 INDIVIDUAL_MATCH_STATISTICS_COLLECTION.
 * INDIVIDUAL_MATCH_STATISTICS_COLLECTION is where the match statistics are stored when a match is extracted by the
 algorithm. It is the document that is referenced by user_account_keys::accounts_list::SAVED_STATISTICS_OID, and it is 
 kept up to date whenever an action occurs on the match element (actions are calls to userMatchOptions()).
 * MATCH_ALGORITHM_RESULTS_COLLECTION will store previously generated days. Each day that has been generated could 
 contain several documents. Each day will always include a header document (with CATEGORIES_TYPE==TYPE_KEY_HEADER_VALUE
 and CATEGORIES_VALUE==VALUE_KEY_HEADER_VALUE) and a document for each activity or category that was swiped on during
 that day (this means that only a header could exist for a given day).

### NOTES
 * There is no reasonable way to know what the users' intention was when they perform a swipe. This means userA could
 see several activities from userB and decide to swipe yes. The way that this is determined is by assuming that on the
 initial algorithm match all activities or categories that were a match in any way were swiped on. For example say
     * userA has Basketball, Theatre and Dancing selected. 
     * userB has Basketball, Theatre and Skiing selected.
 This means that Basketball and Theatre will both have a swipe recorded on that day. Dancing and skiing will not have
 one.
 * The purpose of this system is to avoid consistently compiling all documents inside MATCH_ALGORITHM_RESULTS_COLLECTION
 every time the statistics are requested. While it won't matter to start, it could matter if
 MATCH_ALGORITHM_RESULTS_COLLECTION gets bloated.
 * 'yes_yes' can be used to view the number of completed matches. It can be assumed that whenever this has been
 reached, a match has been made between users.
 * Each match is only counted once. Even when two swipes occur ie yes_yes, yes_no, etc... each of these only count as
 one.
 
---

