//
// Created by jeremiah on 3/12/22.
//

#include "session_to_run_functions.h"

[[maybe_unused]] mongocxx::cursor aggregate_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const mongocxx::pipeline& pipeline,
        const mongocxx::options::aggregate& options
) {
    if (session == nullptr) {
        return collection.aggregate(pipeline, options);
    } else {
        return collection.aggregate(*session, pipeline, options);
    }
}