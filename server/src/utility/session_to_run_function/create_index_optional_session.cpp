//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] bsoncxx::document::value create_index_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& keys,
        const bsoncxx::document::view_or_value& index_options,
        const mongocxx::options::index_view& operation_options
) {
    if (session == nullptr) {
        return collection.create_index(
                keys,
                index_options,
                operation_options
                );
    } else {
        return collection.create_index(
                *session,
                keys,
                index_options,
                operation_options
                );
    }
}