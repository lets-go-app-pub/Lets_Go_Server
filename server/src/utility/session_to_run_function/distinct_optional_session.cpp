//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::cursor distinct_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::string::view_or_value& name,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::distinct& options
) {
    if (session == nullptr) {
        return collection.distinct(name, filter, options);
    } else {
        return collection.distinct(*session, name, filter, options);
    }
}