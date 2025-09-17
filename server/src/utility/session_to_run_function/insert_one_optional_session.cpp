//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::insert_one> insert_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& document,
        const mongocxx::options::insert& options
) {
    if (session == nullptr) {
        return collection.insert_one(document, options);
    } else {
        return collection.insert_one(*session, document, options);
    }
}
