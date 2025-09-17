//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::delete_result> delete_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::delete_options& options
) {
    if (session == nullptr) {
        return collection.delete_one(filter, options);
    } else {
        return collection.delete_one(*session, filter, options);
    }
}