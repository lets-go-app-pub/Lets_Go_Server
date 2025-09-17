//
// Created by jeremiah on 3/12/22.
//

#include <cstdint>

#include <mongocxx/client_session.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/view_or_value.hpp>
#include <mongocxx/options/count.hpp>
#include <mongocxx/index_view.hpp>
#include <mongocxx/result/delete.hpp>
#include <mongocxx/options/delete.hpp>
#include <mongocxx/options/distinct.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/find_one_and_delete.hpp>
#include <mongocxx/options/find_one_and_replace.hpp>
#include <mongocxx/options/find_one_and_update.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/result/insert_one.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/result/replace_one.hpp>
#include <mongocxx/options/replace.hpp>
#include <mongocxx/result/update.hpp>
#include <mongocxx/options/update.hpp>

[[maybe_unused]] mongocxx::cursor aggregate_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const mongocxx::pipeline& pipeline,
        const mongocxx::options::aggregate& options = mongocxx::options::aggregate()
);

[[maybe_unused]] std::int64_t count_documents_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::count& options = mongocxx::options::count()
);

[[maybe_unused]] bsoncxx::document::value create_index_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& keys,
        const bsoncxx::document::view_or_value& index_options = bsoncxx::document::view_or_value{},
        const mongocxx::options::index_view& operation_options = mongocxx::options::index_view{}
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::delete_result> delete_many_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::delete_options& options = mongocxx::options::delete_options()
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::delete_result> delete_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::delete_options& options = mongocxx::options::delete_options()
);

[[maybe_unused]] mongocxx::cursor distinct_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::string::view_or_value& name,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::distinct& options = mongocxx::options::distinct()
);

[[maybe_unused]] mongocxx::cursor find_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::find& options = mongocxx::options::find()
);

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::find& options = mongocxx::options::find()
);

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_delete_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::options::find_one_and_delete& options = mongocxx::options::find_one_and_delete()
);

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_replace_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& replacement,
        const mongocxx::options::find_one_and_replace& options = mongocxx::options::find_one_and_replace()
);

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_update_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& update,
        const mongocxx::options::find_one_and_update& options = mongocxx::options::find_one_and_update()
);

[[maybe_unused]] mongocxx::stdx::optional<bsoncxx::document::value> find_one_and_update_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::pipeline& update,
        const mongocxx::options::find_one_and_update& options = mongocxx::options::find_one_and_update()
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::insert_one> insert_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& document,
        const mongocxx::options::insert& options = mongocxx::options::insert{}
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::replace_one> replace_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& replacement,
        const mongocxx::options::replace& options = mongocxx::options::replace{}
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::update> update_many_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& update,
        const mongocxx::options::update& options = mongocxx::options::update()
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::update> update_many_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::pipeline& update,
        const mongocxx::options::update& options = mongocxx::options::update()
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::update> update_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const bsoncxx::document::view_or_value& update,
        const mongocxx::options::update& options = mongocxx::options::update()
);

[[maybe_unused]] mongocxx::stdx::optional<mongocxx::result::update> update_one_optional_session(
        mongocxx::client_session* session,
        mongocxx::collection& collection,
        const bsoncxx::document::view_or_value& filter,
        const mongocxx::pipeline& update,
        const mongocxx::options::update& options = mongocxx::options::update()
);