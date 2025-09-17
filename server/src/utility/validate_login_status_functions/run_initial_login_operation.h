//
// Created by jeremiah on 9/14/22.
//

#pragma once

#include <mongocxx/client_session.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>

//can pass nullptr to session if not needed
bool runInitialLoginOperation(
        bsoncxx::stdx::optional<bsoncxx::document::value>& findAndUpdateUserAccount,
        mongocxx::collection& userAccountsCollection,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::document::view& loginDocument,
        const bsoncxx::document::view& projectionDocument,
        mongocxx::client_session* session = nullptr
);