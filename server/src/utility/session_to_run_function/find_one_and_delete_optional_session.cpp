//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_delete_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::find_one_and_delete& options
) {
    if (session == nullptr) {
        return collection.find_one_and_delete(filter, options);
    } else {
        return collection.find_one_and_delete(*session, filter, options);
    }
}
