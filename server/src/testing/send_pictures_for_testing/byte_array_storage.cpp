//
// Created by jeremiah on 3/21/21.
//

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <connection_pool_global_variable.h>

#include "send_pictures_for_testing.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

int byteArrayIndexNumber = 0;

void byteArrayStorage(const send_picture_for_testing::SendPicturesForTestingRequest* request,
                      send_picture_for_testing::SendPicturesForTestingResponse* response) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::collection collection = mongoCppClient["Pictures_For_Testing"]["Pics"];

    mongocxx::options::update updateOptions;
    updateOptions.upsert(true);

    bool exceptionThrown = false;

    bsoncxx::stdx::optional<mongocxx::result::update> updateByte;
    try {
        updateByte = collection.update_one(
            document{}
                << "_id" << byteArrayIndexNumber
            << finalize,
            document{}
                << "$set" << open_document
                    << "_id" << byteArrayIndexNumber
                    << "pic" << request->send_bytes()
                    << "thumbnail" << request->send_thumbnail()
                << close_document
            << finalize,
            updateOptions
        );

        std::cout << "index: " << byteArrayIndexNumber << " saved\n";
    } catch (std::exception& e) {
        exceptionThrown = true;
        std::cout << "Exception: " << e.what() << '\n';
    }

    if(!exceptionThrown && !updateByte) {
        std::cout << "Failed to update picture index: " << byteArrayIndexNumber << '\n';
    }

    byteArrayIndexNumber++;
    response->set_received_successfully(true);
}


