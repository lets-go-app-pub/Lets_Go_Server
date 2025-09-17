
## setCategories()

Setting the categories is fairly straightforward from simply looking at the input message (SetCategoriesRequest) and the output
 message (SetFieldResponse). However, there are some nuances involved with how the time frames and activities should be
 sent.
 1) Time frames should be ordered. Earlier time frames should be first in the request list.
 2) Time frames should not have any overlaps. All overlaps that the user selects should be removed before sending to
  the server.

NOTES
 * These rules apply to the database except that the database will remove any times that are before
   (current_time + TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED). This can cause the initial starting time to be
   missing occasionally.
 * The client never checks for overlaps and updates categories, it simply overwrites them every time.
 * More information about timeframes can be found inside find_matches/_documentation.md. 


## How Pictures Work With Accounts

Whenever a user switches a picture OR deletes their account there is a chance that their thumbnail could be updated. 
However, when they are no longer in the chat room I am not going to take this into account. For deleted pictures it 
makes sense, the user would have no thumbnail in every chat room they had ever joined if I implemented it. When 
replacing thumbnails in chat rooms it is a little sketchy but it shouldn't matter much. However, when a picture
is deleted by an admin it will be removed from the user account (possibly leaving 0 pictures) and it will also iterate
through the thumbnails the picture saved inside any chat rooms and delete those as well.

Changing the original 'standard' I set up for pictures of them never being deleted.
 1) It can have an empty array (in the user account document).
 2) It can have 'holes' inside it.
 3) Still never turn the Server Global Variable NUMBER_PICTURES_STORED_PER_ACCOUNT down.
