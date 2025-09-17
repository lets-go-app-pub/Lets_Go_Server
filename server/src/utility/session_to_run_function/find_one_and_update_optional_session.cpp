//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_update_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& update,
        const mongocxx::options::find_one_and_update& options
) {
    if (session == nullptr) {
        return collection.find_one_and_update(filter, update, options);
    } else {
        return collection.find_one_and_update(*session, filter, update, options);
    }
}

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_update_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::pipeline& update,
        const mongocxx::options::find_one_and_update& options
) {
    if (session == nullptr) {
        return collection.find_one_and_update(filter, update, options);
    } else {
        return collection.find_one_and_update(*session, filter, update, options);
    }
}