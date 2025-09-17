### Standard

Inside update_signed_other_user.cpp it will send back specific messages
representing that a picture or thumbnail has been deleted. The functions
for these are setThumbnailToDeleted() for thumbnail and setPictureToDeleted()
for picture.

It is important that these are followed for 'deleted' values. This is because
android will expect these values to be returned.


### Sending Info Back To Users

The client will only keep track of existing pictures along with their info. This
means that it could very easily send in say index 0 and 2 skipping 1. This
is the way that the server should expect info. If say 1 in the example is NOT 
sent in then it is assumed that the client assumes that a picture does not exist at
that index.

The server will check the timestamp value stored inside KEYS_TO_CHECK_TIMESTAMPS as well
as the age. If any of these value require an update, ALL the basic user account info
will be sent back.

* The server will send back the deletePicture message if a picture that the client sends is
deleted.

* The server will send back -1 for the thumbnail index (as well as the deleted pictures) if all picture
for the passed user have been deleted.

* The client can pass in NO pictures and get the full list of pictures from the server.

* If the account state inside the chat room is updated, this will be sent back.

