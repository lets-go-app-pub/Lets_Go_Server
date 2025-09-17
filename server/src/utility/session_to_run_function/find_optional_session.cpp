//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::cursor find_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::find& options
) {
    if (session == nullptr) {
        return collection.find(filter, options);
    } else {
        return collection.find(*session, filter, options);
    }
}