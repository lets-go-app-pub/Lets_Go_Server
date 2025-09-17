
## Flow

Feedback is sent to server by the client using setFeedback() then stored as one of the three types.

 * Activity
 * Bug Report
 * Other

It is then stored inside of one of the respective collections and extracted by the desktop interface.

getInitialFeedback() is called first by the desktop interface, this requires the user to have seen feedback before. 
It will check if the admin has ever seen feedback before. If they have not, it will request the next 
NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT * 2 messages. If they have, it will request the next
NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT messages and the previous 
NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT messages.

The admin can then scroll through the feedback on the desktop interface. When they get near the start or end of their 
list the device will automatically call getFeedbackUpdate() and request an additional
NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE. This process will repeat until it reaches one end of the feedback.

The updateFeedbackTimeViewed() function will update the last time the admin viewed this specific type of feedback. This
will determine which feedback is requested the next time getInitialFeedback() is called.

The setFeedbackToSpam() will set a feedback to spam. It will update the user account to a spammer and when the user
reaches a certain number of spam feedback sent in, any future feedback will be ignored.

---

## Collections

There are three collections related to feedback
 * ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME (special has an extra field for activity name)
 * BUG_REPORT_FEEDBACK_COLLECTION_NAME
 * OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME

Each collection has an index on its own TIMESTAMP_STORED field for use when running getInitialFeedback() and
getFeedbackUpdate().
