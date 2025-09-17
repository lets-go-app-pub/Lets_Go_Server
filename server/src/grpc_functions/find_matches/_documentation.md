
## Flow of algorithm system

1) The algorithm will run a search for people and make sure that BOTH users fit into the
   others search parameters (age range, acceptable genders, max distance etc...). It
   will also make sure that at least one activity or category matches.

2) The algorithm will look at various information and use a point system to order matches, the criteria is below.
    1. Each activity or category that matches will give points.
    2. Timeframe overlaps give points.
    3. If a timeframe does not overlap but is close it will give a small amount of points.
    4. The amount of time a user has been inactive is weighed and points are subtracted based on this.
    5. If the users have matched recently then points will be subtracted.

3) The matches will then be stored inside the 'algorithm match' list.

4) If no matches are found, the algorithm will run again to attempt to find any events in the area. This will ignore
 activity and distance searches.

5) When userA receives a match with userB and swipes 'yes', then the match will be put into userBs 'other user said
 yes' list. For any event that is swiped 'yes' on, the user will automatically be added to the chat room. 

6) If userB receives the match and also swipes 'yes' then userA and userB will successfully match and be put into a new
 chat room together.

7) If a userA swipes 'yes' or 'no' on userB, then userB will be put in a 'recently matched' list. This will prevent
 userA from seeing userB for a short while.

8) If a userA swipes 'block' or 'report' on userB they will be put in a 'restricted accounts' list. This will prevent
 userA from ever seeing userB again. 'block' may not be used in practice because the buttons read 
 'Block & Report'.

## Concept

Conceptually I want people to be able to swipe on others and get as close to a 'real time' feedback as possible. So if
someone wants to do the activity 'Dancing' then the goal is for them to be able to pick Dancing and immediately see
people that also want dancing around the same time. TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED is the variable that
handles this.

## findMatches() input

The protobuf message FindMatchesRequest is the input for the client. 

* login_info is expected to be filled out.
* number_messages will be the number of messages the client is requesting. If the number is greater than
 MAXIMUM_NUMBER_RESPONSE_MESSAGES, then MAXIMUM_NUMBER_RESPONSE_MESSAGES will be requested. While
 requesting zero messages will work just fine, it is discouraged.
* longitude and latitude are always expected to be filled out.

When requesting remember that.
 * Number swipes remaining are calculated when each match is **extracted** not after the user swipes on it.
 * The algorithm is expensive in terms of resources.

A conclusion of these points is to avoid spamming the findMatches() function. The client is expected to request once 
and use all info it receives.

## findMatches() output

The protobuf message FindMatchesResponse is the input for the client. There are two types of messages that can be 
returned with it.

1) SingleMatchMessage; A successful match and the information associated with the user.
2) FindMatchesCapMessage; Conceptually the 'cap' of the server side stream. This is guaranteed to be sent back assuming
 no connection error occurs and guaranteed to be the final message of the stream.

FindMatchesCapMessage will be used to send back errors that occur using the return_status field. When return_status is
set to SUCCESS then the success_type field will be used to relay information. There are a few possibilities

 1) UNKNOWN; Should never be returned if return_status == SUCCESS.
 2) SUCCESSFULLY_EXTRACTED; Will be set if the proper number of requests (number_messages inside FindMatchesRequest) 
 was returned to the client.
 3) NO_MATCHES_FOUND; This means that the algorithm ran and not enough matches could be found to meet the number of 
 requested matches.
 4) MATCH_ALGORITHM_ON_COOL_DOWN; This means the algorithm was on cool down. There are three possible times this could
 happen.
    1) The algorithm ran within the last TIME_BETWEEN_ALGORITHM_RUNS.
    2) The algorithm returned NO_MATCHES_FOUND within the last TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.
    3) user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK is still 'locked'.
 5) NO_SWIPES_REMAINING; This means the user ran out of swipes before or during extracting the matches.

Other fields inside FindMatchesCapMessage are as follows

 * swipes_time_before_reset will be sent back if return_status == SUCCESS and success_type == SUCCESSFULLY_EXTRACTED,
 then it will be the time before the swipes for the client are reset in milliseconds.
 * cool_down_on_match_algorithm will be sent back if success_type == NO_MATCHES_FOUND or
 success_type == MATCH_ALGORITHM_ON_COOL_DOWN.
 * timestamp will be sent back if return_status == SUCCESS is set. It will also occasionally be sent back when 
 return_status is set to a different values.

SingleMatchMessage will be sent back representing a match that was found. Fields include
 * member_info is the user info of the member being sent back.
 * point_value is the point value representation of the match.
 * expiration_time is the time the match should be removed from the client (see below for details).
 * other_user_match will be true if the matched user already swiped yes on the current user.
 * swipes_remaining is the total number of swipes remaining after this match was extracted.
 * swipes_time_before_reset is the time before the swipes for the client are reset in milliseconds.
 * timestamp is the timestamp the function started on the server.

## Expiration time

Expiration time is conceptually exactly as the name implies, it is the time the match expires. There are however, some
nuances worth mentioning. The expiration time is calculated at the time the algorithm runs along with other things
such as the point value of the match. It can vary depending on why the users matched. The possibilities are

 1) If an overlap time occurs it is set to the end of the overlap.
 2) If between times occur it is set to the end of the first time frame.
 3) If no between or overlap times occur it is set to the maximum value possible for that match.
 4) When the match is extracted and sent back to the client, the expiration time will be set to a maximum of 
 matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE time if it was popped from 
 OTHER_USERS_MATCHED_ACCOUNTS_LIST.

A few more points
* If a user has 'anytime' selected, then the algorithm simply counts this as a very large timeframe. This means that two
 users that match with 'anytime' selected will have a large expiration time.
* If multiple overlapping or between times occur for a single activity, the latest time is used.

## Notes

Some general notes related to the client server interaction.
* The algorithm could take a while, it is recommended that the deadline time for gRPC is fairly long.
* There are times when the algorithm is cleared, and it loses all of its extracted matches (for example if gender
 is set). The client is expected to mirror these times and clear the matches it stores as well. See
 appendDocumentToClearMatchingInfoAggregation() functions in the code for locations that arrays are cleared.

There are 2 ways I can calculate swipes
 1) When the swipe is sent BACK to the server.
    * PRO: this will give the user the 'feel' of having more swipes
    * CON:  This will allow people to take advantage of the situation, for example if no swipes are sent back (a 3rd
      party program) someone could get infinite swipes. Or more reasonably they could select an activity, NOT swipe yes
      or no, then select another activity.
 2) When the swipe is extracted FROM the server.
    * PRO : It prevents the CON from 1.
    * CON : The user will 'use' swipes much faster like this.

---

## Algorithm internals

This will attempt to record some nuances of the algorithm implementation for later. It is meant more as a general 
reference than a guide to the algorithm as a whole. Other comments are scattered through the process in order to
give specific reasons for certain actions. The algorithm itself is stored inside a javascript file inside the C++
project 'GenerateAlgorithmAndStuff'.

### Accuracy vs Quantity

This is a major sticking point of decision-making in designing the algorithm. There are many places in which a more
accurate (and possibly closer to what the user would expect) result could be returned. However, especially in the early
stages of the app quantity of matches is prioritized. Some decisions that need to be (and have been) made are
 * When appendDocumentToClearMatchingInfoAggregation() is run, should it retain matches?
 * Inside generateSwipedYesOnUserMatchDocument() and generateValidateArrayElementMatchDocument() several things are
 commented out that would provide a more consistent experience.
 * Inside generateCategoriesActivitiesMatching() SEARCH_BY_OPTIONS is checked if the user is searching for categories.
However, at the heart of it anything that is a part of these systems or the algorithm query itself can be changed to
get more matches at the cost of 'accuracy'.

### Algorithm query

Searched for in query (sudo code)
 * user_account_keys::STATUS == UserAccountStatus::STATUS_ACTIVE
 * user_account_keys::MATCHING_ACTIVATED == true
 * user_account_keys::age_range::MIN(match) <= user_age <= user_account_keys::age_range::MAX(match)
 * user_account_keys::age_range::MIN(user) <= match_age <= user_account_keys::age_range::MAX(user)
 * user_account_keys::GENDER(user) in user_account_keys::GENDERS_RANGE(match)
 * user_account_keys::GENDER(match) in user_account_keys::GENDERS_RANGE(user)
 * _id(user) not in user_account_keys::OTHER_USERS_BLOCKED(match)
 * _id(user) not recent in user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS(match) (based on TIME_BETWEEN_SAME_ACCOUNT_MATCHES)
 * match is within user_account_keys::MAX_DISTANCE(user)
 * user is within user_account_keys::MAX_DISTANCE(match)
 * restricted ids (extracted from user account): 
     * _id
     * user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS (based on TIME_BETWEEN_SAME_ACCOUNT_MATCHES)
     * user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
     * user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
     * user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST
     * user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH
     * user_account_keys::OTHER_USERS_BLOCKED
 * (if AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY)
     * any matching activity
 * (if AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY)
     * any matching category
     * user_account_keys::SEARCH_BY_OPTIONS == AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY

### Point calculations

* OVERLAPPING_TIMES_WEIGHT represents either OVERLAPPING_ACTIVITY_TIMES_WEIGHT or OVERLAPPING_CATEGORY_TIMES_WEIGHT 
* SHORT_TIMEFRAME_OVERLAP_WEIGHT represents either SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT or SHORT_TIMEFRAME_CATEGORY_OVERLAP_WEIGHT 
* BETWEEN_TIMES_WEIGHT represents either BETWEEN_ACTIVITY_TIMES_WEIGHT or BETWEEN_CATEGORY_TIMES_WEIGHT 

First the query above is run to eliminate invalid matches. After that the algorithm is run on the remaining matches and
assigns each match (matching user) a point value. The higher the point value, the 'better' the match is supposed to be.
There is no minimum or maximum value that the points can come out, it is simply a double. The 'category' matches are
set up to be order(s) of magnitude below 'activity' matches for point values, however they follow the same calculations. The simple
explanation is given above under header 'Flow of algorithm system', they are repeated and discussed here.

 1. **Result is added**; Each activity or category that matches will give points.
     * These are the simplest points to understand. A matching category will give a flat CATEGORIES_MATCH_WEIGHT and a
     matching activity will give a flat ACTIVITY_MATCH_WEIGHT.
 2. **Result is added**; Timeframe overlaps give points.
     * It first determines if the user or the document has a larger total timeFrame (calling the larger value
     **chosenTimeFrame**) Example: userA timeframes: [3-6, 7-11]; userB timeframes: [4-5]. The means userA total 
     timeFrame would be 6-3 + 11-7 = 7 userB total timeFrame would be 5-4 = 1 so userA has a larger timeFrame because
     7 > 1 (startTime and stopTime excluded for brevity).
     * It will then calculate a value for giving extra weight to smaller timeframes. This is because a match that is
     say 5:00 - 6:00 is expected to be a 'better' match than a match of 'anytime'. And this weight will attempt to
     account for that. The calculation for this value (called **shortOverlapPoints**) is <br/>
     SHORT_TIMEFRAME_OVERLAP_WEIGHT * (1 - totalOverlapTime/totalTimeFrame).
     * This will make the final equation of<br/>
     (shortOverlapPoints + OVERLAPPING_TIMES_WEIGHT) * totalOverlapTime/chosenTimeFrame.
     * Each activity will have its own point value and these point values will be summed to calculate Overlapping points.
 3. **Result is added**; If a timeframe does not overlap but is close it will give a small amount of points.
    * If a timeframe of the current user is within MAX_BETWEEN_TIME_WEIGHT_HELPER of a timeframe of the matching user
    then the time between the timeframes will be stored (called **betweenTime**).
    * The equation is<br/>
    BETWEEN_TIMES_WEIGHT * (1 - betweenTime/MAX_BETWEEN_TIME_WEIGHT_HELPER)
    * Each activity can have multiple between times weights associated with it and each will be added to the cumulative
    point score.
 4. **Result is subtracted**; The amount of time a user has been inactive is weighed and points are subtracted based on this.
    * This is based on the matching users' LAST_TIME_FIND_MATCHES_RAN value. The longer the matching user has not run
    the algorithm, the more points will be subtracted.
    * The equation is<br/>
    INACTIVE_ACCOUNT_WEIGHT * (current_timestamp - LAST_TIME_FIND_MATCHES_RAN)
 5. **Result is subtracted**; If the users have matched recently then points will be subtracted.
    * Conceptually the idea behind this is fairly straightforward. After PREVIOUSLY_MATCHED_FALLOFF_TIME has passed
    there will be no points subtracted anymore from the total points of the match. However, each match will extend this
    linearly for example after 2 times the accounts have matched it will be 2 * PREVIOUSLY_MATCHED_FALLOFF_TIME after 3
    it will be 3 * PREVIOUSLY_MATCHED_FALLOFF_TIME etc.
    * The equation is<br/>
      PREVIOUSLY_MATCHED_WEIGHT * (1 - timeSinceLastMatch/(PREVIOUSLY_MATCHED_FALLOFF_TIME * numberTimesMatched))
    * **timeSinceLastMatch** = current timestamp - previously_matched_accounts::TIMESTAMP
    * **numberTimesMatched** = previously_matched_accounts::NUMBER_TIMES_MATCHED
    * If timeSinceLastMatch/(PREVIOUSLY_MATCHED_FALLOFF_TIME * numberTimesMatched) > 1 then this weight will not be
    taken into account.
    
### Extra Notes

 * The time frames inside user_account_keys::CATEGORIES are expected to be ordered from smallest to largest and to not
 have any overlaps. However, the algorithm automatically removes them if they are in the past. This means the array can
 be empty or contain a stop time without its respective start time. 
 * During the algorithm 1 is added to user_account_values.earliest_time_frame_start_timestamp. This is to avoid the edge
 case where a time could exist specifically at this timestamp (for example the final stop time of a time frame). This
 situation could create a timeframe of size 0.
 * If AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY is set then the user will check if there are any
 matching activities. If there are matching activities they will only match by activity (exactly the same as 
 AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY). If there are no matching activities it will run the algorithm to
 only match by category (activities will not be taken into account except as an edge case).
 * A situation can occur when a user swipes yes on someone. This will cause the matched user to be added to the 
 matches 'other user said yes' vector. However, if the match reaches the expiration time before ever getting swiped,
 then the original user missed out because they now have a value added to PREVIOUSLY_MATCHED_ACCOUNTS. But if I go back
 and remove it from their vector they can get view the same user multiple times, so leaving it the way it is, they can
 match again later. This is subject to change.
