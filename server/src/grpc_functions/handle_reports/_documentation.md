
## Flow

1) The first time a user (say userA) is reported by another user, it will create a document inside
OUTSTANDING_REPORTS_COLLECTION and add the first element to the REPORTS_LOG array.
2) Each time after this userA is reported by a different user (if the same user attempts a report, it will not be
stored) the REPORTS_LOG array will grow one size larger.
3) When the array size reaches NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED, TIMESTAMP_LIMIT_REACHED will be set to the
timestamp. 
4) requestReports() will now find this report and return it to the desktop interface when it is run. There are three
possible actions the desktop interface can take for this report.
    1) timeOutUser(); This will time out the reported user. The first time a user is timed out they will get a short 
    suspension. Each time a user is timed out after that the suspension time will increase until eventually they will be
    banned. Note that a user that is currently timed out cannot be timed out again.
    2) setReportToSpam(); This means that a reporting user is just spamming. Such as reporting the user when they did
    nothing wrong.
    3) dismissReport(); This will move the report from the OUTSTANDING_REPORTS_COLLECTION to the
    HANDLED_REPORTS_COLLECTION.

---

## Reports sent from

Initially sent to the server by the client, currently all reports go through the buildUpdateReportLogDocument() function.
However, there is no guarantee of this in the future. Reports can be sent to the server in two ways.

1) When a user is swiping, block & report is an option.
2) When a user is inside a chat room they can block & report another user.

---

## Collections

There are two collections involved with reports.
 * OUTSTANDING_REPORTS_COLLECTION
 * HANDLED_REPORTS_COLLECTION

Indexes
 * OUTSTANDING_REPORTS_COLLECTION is indexed on TIMESTAMP_LIMIT_REACHED in order to extract reports inside 
 requestReports().
 * HANDLED_REPORTS_COLLECTION has no custom indexes.