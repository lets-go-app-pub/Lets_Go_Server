//
// Created by jeremiah on 9/12/21.
//

#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <SetFields.grpc.pb.h>

bool checkForUpdateUserPrivilege(std::basic_string<char>* error_str,
                                 const bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc_value);

void updateUserAccountWithDocument(const std::string& userAccountOIDStr, const bsoncxx::document::view& mergeDocument,
                                   const std::function<void()>& success_func,
                                   const std::function<void(const std::string& /* error_str */)>& error_func,
                                   const std::function<void(mongocxx::client&,
                                                            mongocxx::database&)>& save_statistics = nullptr);
