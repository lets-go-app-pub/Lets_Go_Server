//
// Created by jeremiah on 6/17/22.
//

#include <fstream>
#include <mongocxx/pool.hpp>
#include <reports_objects.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>
#include <user_pictures_keys.h>

#include <mongo_oid_concurrent_container.h>
#include <chat_stream_container_object.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class MongoOidConcurrentContainer : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_insert) {
    MongoDBOIDContainer dummy_chat_room_map;
    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = "507f191e810c19729de860ea";

    int firstIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 1]);
    int secondIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 2]);
    int thirdIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 3]);

    MongoDBOIDContainer::MapWrapper& map_wrapper = dummy_chat_room_map.container[firstIndex][secondIndex][thirdIndex];

    EXPECT_EQ(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    //upsert first value which will set map to an object and not nullptr
    dummy_chat_room_map.insert(dummy_oid, &dummy_container);

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_TRUE((*map_wrapper.map).contains(dummy_oid));

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = "607f191e810c19729de860ea"; //same as dummy_oid with first digit incremented

    //upsert second value which will use the generated map and add the new value
    dummy_chat_room_map.insert(dummy_oid_two, &dummy_container_two);

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_TRUE((*map_wrapper.map).contains(dummy_oid_two));

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);
}

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_upsert) {
    MongoDBOIDContainer dummy_chat_room_map;
    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = "507f191e810c19729de860ea";
    dummy_container.current_user_account_oid_str = dummy_oid;

    bool function_ran = false;
    auto function_to_run_while_locked =  [&](ChatStreamContainerObject*) {
        function_ran = true;
    };

    int firstIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 1]);
    int secondIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 2]);
    int thirdIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 3]);

    MongoDBOIDContainer::MapWrapper& map_wrapper = dummy_chat_room_map.container[firstIndex][secondIndex][thirdIndex];

    EXPECT_EQ(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    bool successful = false;
    //upsert first value which will set map to an object and not nullptr
    auto handle = dummy_chat_room_map.upsert(dummy_oid, &dummy_container, successful, function_to_run_while_locked).handle;
    handle.resume();
    handle.destroy();

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_TRUE((*map_wrapper.map).contains(dummy_oid));

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = "607f191e810c19729de860ea"; //same as dummy_oid with first digit incremented
    dummy_container_two.current_user_account_oid_str = dummy_oid_two;

    //replace the same value with dummy_container_two
    handle = dummy_chat_room_map.upsert(dummy_oid, &dummy_container_two, successful, function_to_run_while_locked).handle;
    handle.resume();
    handle.destroy();

    ASSERT_NE(map_wrapper.map, nullptr);

    EXPECT_TRUE(function_ran);

    EXPECT_EQ((*map_wrapper.map)[dummy_oid]->current_user_account_oid_str, dummy_oid_two);

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);

    //add a new value
    handle = dummy_chat_room_map.upsert(dummy_oid_two, &dummy_container, successful, function_to_run_while_locked).handle;
    handle.resume();
    handle.destroy();

    ASSERT_NE(map_wrapper.map, nullptr);

    EXPECT_EQ((*map_wrapper.map)[dummy_oid_two]->current_user_account_oid_str, dummy_oid);

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);
}

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_erase) {
    MongoDBOIDContainer dummy_chat_room_map;

    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = "507f191e810c19729de860ea";

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = "607f191e810c19729de860ea"; //same as dummy_oid with first digit incremented

    int firstIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 1]);
    int secondIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 2]);
    int thirdIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 3]);

    MongoDBOIDContainer::MapWrapper& map_wrapper = dummy_chat_room_map.container[firstIndex][secondIndex][thirdIndex];

    EXPECT_EQ(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    dummy_chat_room_map.insert(dummy_oid, &dummy_container);
    dummy_chat_room_map.insert(dummy_oid_two, &dummy_container_two);

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);

    dummy_chat_room_map.erase(dummy_oid);

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);
    EXPECT_FALSE((*map_wrapper.map).contains(dummy_oid));

    dummy_chat_room_map.erase(dummy_oid_two);

    EXPECT_EQ(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

}

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_eraseWithCondition) {
    MongoDBOIDContainer dummy_chat_room_map;

    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = "507f191e810c19729de860ea";

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = "607f191e810c19729de860ea"; //same as dummy_oid with first digit incremented

    int firstIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 1]);
    int secondIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 2]);
    int thirdIndex = MongoDBOIDContainer::getIndex(dummy_oid[dummy_oid.size() - 3]);

    MongoDBOIDContainer::MapWrapper& map_wrapper = dummy_chat_room_map.container[firstIndex][secondIndex][thirdIndex];

    EXPECT_EQ(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    dummy_chat_room_map.insert(dummy_oid, &dummy_container);
    dummy_chat_room_map.insert(dummy_oid_two, &dummy_container_two);

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);

    bool successful = false;
    auto handle = dummy_chat_room_map.eraseWithCondition(dummy_oid, successful, [](const ChatStreamContainerObject*){return true;}).handle;
    handle.resume();
    handle.destroy();

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);
    EXPECT_FALSE((*map_wrapper.map).contains(dummy_oid));

    handle = dummy_chat_room_map.eraseWithCondition(dummy_oid_two, successful, [](const ChatStreamContainerObject*){return false;}).handle;
    handle.resume();
    handle.destroy();

    ASSERT_NE(map_wrapper.map, nullptr);
    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 1);
    EXPECT_FALSE((*map_wrapper.map).contains(dummy_oid));
}

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_find) {

    MongoDBOIDContainer dummy_chat_room_map;

    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = bsoncxx::oid{}.to_string();
    dummy_container.current_user_account_oid_str = dummy_oid;

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = bsoncxx::oid{}.to_string();
    dummy_container_two.current_user_account_oid_str = dummy_oid_two;

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    dummy_chat_room_map.insert(dummy_oid, &dummy_container);
    dummy_chat_room_map.insert(dummy_oid_two, &dummy_container_two);

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);

    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> result = dummy_chat_room_map.find(dummy_oid);
    EXPECT_EQ(result->ptr()->current_user_account_oid_str, dummy_oid);

    result = dummy_chat_room_map.find(dummy_oid_two);
    EXPECT_EQ(result->ptr()->current_user_account_oid_str, dummy_oid_two);

    result = dummy_chat_room_map.find(bsoncxx::oid{}.to_string());
    EXPECT_EQ(result, nullptr);
}

TEST_F(MongoOidConcurrentContainer, mongoDBOIDContainer_runCommandOnAllOid) {
    MongoDBOIDContainer dummy_chat_room_map;

    ChatStreamContainerObject dummy_container;
    std::string dummy_oid = bsoncxx::oid{}.to_string();
    dummy_container.current_user_account_oid_str = dummy_oid;

    ChatStreamContainerObject dummy_container_two;
    std::string dummy_oid_two = bsoncxx::oid{}.to_string();
    dummy_container_two.current_user_account_oid_str = dummy_oid_two;

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 0);

    dummy_chat_room_map.insert(dummy_oid, &dummy_container);
    dummy_chat_room_map.insert(dummy_oid_two, &dummy_container_two);

    EXPECT_EQ(dummy_chat_room_map.num_values_stored, 2);

    dummy_chat_room_map.runCommandOnAllOid(
            [](ChatStreamContainerObject* obj){
                obj->current_user_account_oid_str = "1";
            }
    );

    EXPECT_EQ(dummy_container.current_user_account_oid_str, "1");
    EXPECT_EQ(dummy_container_two.current_user_account_oid_str, "1");
}



