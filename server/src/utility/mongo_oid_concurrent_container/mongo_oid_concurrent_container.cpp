//
// Created by jeremiah on 5/9/21.
//

#include "mongo_oid_concurrent_container.h"
#include "chat_stream_container_object.h"

bool MongoDBOIDContainer::insert(const std::string& key, ChatStreamContainerObject* value) {

    MapWrapper* element = nullptr;

    if (getElement(key, &element)) {
        size_t values_added;
        {
            SpinlockWrapper lock(element->spin_lock);
            size_t initial_map_size = 0;
            if (element->map == nullptr) {
                element->map = new std::map<std::string, ChatStreamContainerObject*>{
                    std::make_pair(key, value)
                };
            } else {
                initial_map_size = element->map->size();
                element->map->insert(std::make_pair(key, value));
            }

            //if an element was added, this will be 1, if replacing will be 0
            values_added = element->map->size() - initial_map_size;
        }
        num_values_stored += values_added;
    }
    else {
        return false;
    }

    return true;
}

ThreadPoolSuspend MongoDBOIDContainer::upsert(
        const std::string& key,
        ChatStreamContainerObject* value,
        bool& successful,
        std::function<void(ChatStreamContainerObject*)> function_to_run_while_locked
        ) {

    MapWrapper* element = nullptr;

    if (getElement(key, &element)) {

        size_t num_values_added = 0;
        {
            SCOPED_SPIN_LOCK(element->spin_lock);

            if (element->map == nullptr) { //if no map exists
                element->map = new std::map<std::string, ChatStreamContainerObject*>;
                element->map->insert(std::make_pair(key, value));
                if(function_to_run_while_locked)
                    function_to_run_while_locked(nullptr);
                num_values_added++;
            } else { //if map exists

                auto result = element->map->find(key);

                if (result != element->map->end()) { //oid exists
                    ChatStreamContainerObject* old_oid = result->second;
                    result->second = value;

                    //NOTE: order is important here, the new chatroom function needs to be upserted before the old
                    // one is canceled, this way there should be no gap in the user receiving messages.
                    if(function_to_run_while_locked)
                        function_to_run_while_locked(old_oid);

                } else { //oid does not exist
                    element->map->insert(std::make_pair(key, value));
                    if(function_to_run_while_locked)
                        function_to_run_while_locked(nullptr);
                    num_values_added++;
                }
            }
        }

        num_values_stored += num_values_added;
    }
    else {
        //Don't allow compiler to optimize the lambda to be a const reference, inside a coroutine
        // it can hold a reference to a temporary.
        if(rand() < 0) {
            auto volatile temp = std::move(function_to_run_while_locked);
        }
        successful = false;
    }

    successful = true;

    co_return;
}

size_t MongoDBOIDContainer::private_erase(
        const std::string& key,
        MapWrapper* element,
        const std::function<bool(const ChatStreamContainerObject*)>& objectCheck
        ) {

    size_t num_erased = 0;

    if (element->map != nullptr) {

        auto result = element->map->find(key);

        if (result != element->map->end()) { //if result exists in map

            const ChatStreamContainerObject* ptr = result->second;

            if (objectCheck(ptr)) { //if result passes condition

                num_erased = element->map->erase(key);

                if (element->map->empty()) {
                    delete element->map;
                    element->map = nullptr;
                }
            }
        }
    }

    return num_erased;
}

bool MongoDBOIDContainer::erase(const std::string& key) {
    MapWrapper* element = nullptr;

    if (getElement(key, &element)) {

        size_t num_erased;
        {
            SpinlockWrapper lock(element->spin_lock);
            num_erased = private_erase(
                    key,
                    element,
                    [](const ChatStreamContainerObject*){ return true; }
            );
        }
        num_values_stored -= num_erased;

    } else {
        return false;
    }

    return true;
}

ThreadPoolSuspend MongoDBOIDContainer::eraseWithCondition(
        const std::string& key,
        bool& successful,
        std::function<bool(const ChatStreamContainerObject*)> object_check
) {

    MapWrapper* element = nullptr;

    if (getElement(key, &element)) {
        size_t num_erased;
        {
            SCOPED_SPIN_LOCK(element->spin_lock);
            num_erased = private_erase(key, element, object_check);
        }
        num_values_stored -= num_erased;
    } else {
        successful = false;
    }

    //Don't allow compiler to optimize the lambda to be a const reference, inside a coroutine
    // it can hold a reference to a temporary.
    if(rand() < 0) {
        auto volatile temp = std::move(object_check);
    }

    successful = true;

    co_return;
}

std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> MongoDBOIDContainer::find(const std::string& key) {
    MapWrapper* element = nullptr;

    getElement(key, &element);

    SpinlockWrapper lock(element->spin_lock);

    //this check must be here AND inside the lock because the element could have changed before the mutex was locked
    if (element != nullptr && element->map != nullptr) {

        auto result = element->map->find(key);

        if (result != element->map->end()) {
            //the retrieveReferenceWrapperForCallData can return nullptr if no
            // data is available
            return result->second->retrieveReferenceWrapperForCallData();
        }
    }

    return nullptr;
}

void MongoDBOIDContainer::runCommandOnAllOid(const std::function<void(ChatStreamContainerObject*)>& block) {
    for (auto& i : container) {
        for (auto& j : i) {
            for (auto& k : j) {
                //locking this inside the loop not outside to allow other threads to have access in between
                SpinlockWrapper lock(k.spin_lock);
                if (k.map != nullptr) {
                    for (auto& call_data : *k.map) {
                        block(call_data.second);
                    }
                }
            }
        }
    }
}

#ifdef LG_TESTING
    void MongoDBOIDContainer::clearAll() {
        for(auto& i : container) {
            for(auto& j : i) {
                for(auto& val : j) {
                    if(val.map != nullptr) {
                        val.map->clear();
                        val.map = nullptr;
                    }
                }
            }
        }
        num_values_stored = 0;
    }
#endif