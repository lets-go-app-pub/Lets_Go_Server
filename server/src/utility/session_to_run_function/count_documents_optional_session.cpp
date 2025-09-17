//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] std::int64_t count_documents_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::count& options
) {
    if (session == nullptr) {
        return collection.count_documents(filter, options);
    } else {
        return collection.count_documents(*session, filter, options);
    }
}