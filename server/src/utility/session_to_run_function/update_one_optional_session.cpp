//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional <mongocxx::result::update> update_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& update,
        const mongocxx::options::update& options
) {
    if (session == nullptr) {
        return collection.update_one(filter, update, options);
    } else {
        return collection.update_one(*session, filter, update, options);
    }
}

[[maybe_unused]] mongocxx::stdx::optional <mongocxx::result::update> update_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::pipeline& update,
        const mongocxx::options::update& options
) {
    if (session == nullptr) {
        return collection.update_one(filter, update, options);
    } else {
        return collection.update_one(*session, filter, update, options);
    }
}
