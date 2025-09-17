//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::replace_one> replace_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& replacement,
        const mongocxx::options::replace& options
) {
    if (session == nullptr) {
        return collection.replace_one(filter, replacement, options);
    } else {
        return collection.replace_one(*session, filter, replacement, options);
    }
}