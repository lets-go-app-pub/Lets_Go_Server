
## Flow

1) HANDLED_ERRORS_LIST_COLLECTION_NAME will be checked first, if the error 'type' exists inside then the error will
be ignored. If the 'type' does not exist, errors will be stored inside HANDLED_ERRORS_COLLECTION_NAME. These are the 
errors the desktop interface can extract allowing the user to view them.
2) There are two paths that can be taken now
    1) An individual error can be deleted (say it is irrelevant or spammy) from the desktop interface. This will cause 
    it to be moved to the HANDLED_ERRORS_COLLECTION_NAME. 
    2) An error can be set to handled. This will set the entire error 'type' to handled. It identifies the type by the 
    index on HANDLED_ERRORS_LIST_COLLECTION_NAME. It is essentially all info that will not change unless a version has 
    changed. When this happens all errors of that 'type' are moved to the HANDLED_ERRORS_COLLECTION_NAME. And an entry 
    is added to HANDLED_ERRORS_LIST_COLLECTION_NAME to identify that this error should no longer be stored.

---

## Extracting Errors

The errors are first 'searched for' from FRESH_ERRORS_COLLECTION. This will extract and send back each individual combo
of {ERROR_ORIGIN . VERSION_NUMBER . FILE_NAME . LINE_NUMBER} in order for the user to view the different errors in 
aggregate. It is possible to specify criteria of each value when searching or just return all errors.

When a group of errors is selected, all errors matching the {ERROR_ORIGIN . VERSION_NUMBER . FILE_NAME . LINE_NUMBER} 
will be extracted FRESH_ERRORS_COLLECTION. They will then be sorted using the TIMESTAMP_STORED in descending (the newest
first) and sent back.

---

## Collections Involved

### FRESH_ERRORS_COLLECTION_NAME

This collection is where errors are initially stored.

Indexes
 * TIMESTAMP_STORED in descending order.
 * Compound index of {ERROR_ORIGIN . VERSION_NUMBER . FILE_NAME . LINE_NUMBER . ERROR_URGENCY . DEVICE_NAME . 
 TIMESTAMP_STORED . API_NUMBER}. This index is the same as the HANDLED_ERRORS_COLLECTION_NAME index and the
 first four are the same as the HANDLED_ERRORS_LIST_COLLECTION_NAME index.

NOTE: FRESH_ERRORS_COLLECTION_NAME was used as a test for validation (see setup_mongo_db_indexing.cpp and search for
"validator"), will see how it goes and maybe add validators more later.

### HANDLED_ERRORS_COLLECTION_NAME

When an individual document containing an error is 'deleted' it is moved here. This can happen when an error is set
to handled and all errors of that type are moved or when a specific error is deleted.

Index
* Compound index of {ERROR_ORIGIN . VERSION_NUMBER . FILE_NAME . LINE_NUMBER . ERROR_URGENCY . DEVICE_NAME .
  TIMESTAMP_STORED . API_NUMBER}. This index is the same as the FRESH_ERRORS_COLLECTION_NAME index and the
  first four are the same as the HANDLED_ERRORS_LIST_COLLECTION_NAME index.

### HANDLED_ERRORS_LIST_COLLECTION_NAME

Used to identify errors that have already been set to handled. Does this with the compound index that identifies error 
'type'. This is the specifics of the error based on version number and so it should guarantee uniqueness.

Index
* Compound index of {ERROR_ORIGIN . VERSION_NUMBER . FILE_NAME . LINE_NUMBER}. This index is the same as the first
 four of the FRESH_ERRORS_COLLECTION_NAME and HANDLED_ERRORS_COLLECTION_NAME indexes. 

