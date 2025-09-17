#pragma once

#include <string>
#include <memory>
#include <map>
#include <shared_mutex>
#include <utility>
#include <atomic>
#include <utility_general_functions.h>

#include "reference_wrapper.h"

#ifdef LG_TESTING
#include <gtest/gtest_prod.h>
#endif

#include <coroutine_type.h>
#include <coroutine_spin_lock.h>

class ChatStreamContainerObject;

//This class will store users concurrently
//This class takes in a mongoDB OID as a key and stores a shared pointer to an object.
//if the key is not a 24char string, upsert and erase will return false and find will return nullptr
//if the key does not exist find() will return nullptr, otherwise it will return a shared pointer to the data
class MongoDBOIDContainer {

public:

    /** IMPORTANT: Do not lock map_of_chat_rooms_to_users from inside this class.
     * Inside chat_stream_concurrent_map.h iterateAndSendMessageExcludeSender locks then accesses find. This means
     * that deadlock is a possibility if map_of_chat_rooms_to_users is locked from inside here.**/

    //inserts the value to a map returns false if an invalid key is passed
    //NOTE: use upsert if element may already exist
    bool insert(const std::string& key, ChatStreamContainerObject* value);

    //upserts the value to a map returns false if an invalid key is passed
    /** function_to_run_while_locked is called inside of a mutex, do not call other locking stuff inside of it.**/
    ThreadPoolSuspend upsert(
        const std::string& key,
        ChatStreamContainerObject* value,
        bool& successful,
        std::function<void(ChatStreamContainerObject*)> function_to_run_while_locked
    );

    //erases the value at the key, if an invalid key is passed returns false
    bool erase(const std::string& key);

    //erases the value at the key based on the return val of the lambda
    //when the lambda returns true the value will be erased, if false it will not be erased
    /** DO NOT PUT ANY ACCESS TO map_of_chat_rooms_to_users inside the lambda here (and check any
      * other mutex being locked for deadlock) **/
    ThreadPoolSuspend eraseWithCondition(
            const std::string& key,
            bool& successful,
            std::function<bool(const ChatStreamContainerObject*)> object_check
    );

    //returns the stored shared pointer for the key
    //if the key is incompatible or the element is not found this will return nullptr
    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> find(const std::string& key);

    //will run the passed function on every element of the map (this is not efficient, it is currently
    // only used for server shutdown, probably keep it that way)
    void runCommandOnAllOid(const std::function<void(ChatStreamContainerObject*)>& block);

#ifdef LG_TESTING
    //erases all values
    /** THIS IS NOT THREAD SAFE, IT IS FOR TESTING PURPOSES ONLY**/
    void clearAll();
#endif

private:

    struct MapWrapper {
        CoroutineSpinlock spin_lock;
        std::map<std::string, ChatStreamContainerObject*>* map = nullptr;
    };

    static const int SIZE_OF_ARRAY_BLOCK = 16;

    MapWrapper container[SIZE_OF_ARRAY_BLOCK][SIZE_OF_ARRAY_BLOCK][SIZE_OF_ARRAY_BLOCK];

    std::atomic_uint64_t num_values_stored = 0;

    //takes an a mongodb oid
    bool getElement(const std::string& key, MapWrapper** element) {

        if (isInvalidOIDString(key)) { //size of a mongoDB OID
            *element = nullptr;
            return false;
        }

        int firstIndex = getIndex(key[key.size() - 1]);
        int secondIndex = getIndex(key[key.size() - 2]);
        int thirdIndex = getIndex(key[key.size() - 3]);

        if(firstIndex >= SIZE_OF_ARRAY_BLOCK || secondIndex >= SIZE_OF_ARRAY_BLOCK || thirdIndex >= SIZE_OF_ARRAY_BLOCK) {
            return false;
        }

        *element = &container[firstIndex][secondIndex][thirdIndex];

        return true;
    }

    static int getIndex(char mongoIDChar) {
        if (mongoIDChar > 'F') {
            return mongoIDChar - 87; //lower case letters
        } else if (mongoIDChar > '9') {
            return mongoIDChar - 55; //upper case letters
        } else {
            return mongoIDChar - 48; //numbers
        }
    }

    //Lock is expected to be locked for this.
    //erases the value at the key, if an invalid key is passed returns false
    static size_t private_erase(
        const std::string& key,
        MapWrapper* element,
        const std::function<bool(const ChatStreamContainerObject*)>& objectCheck
    );

#ifdef LG_TESTING
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_insert);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_upsert);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_erase);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_eraseWithCondition);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_find);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_runCommandOnAllOid);

    FRIEND_TEST(ServerStressTest, small_stress_test);
    FRIEND_TEST(ServerStressTest, joined_left_chat_room_test);
#endif
};

