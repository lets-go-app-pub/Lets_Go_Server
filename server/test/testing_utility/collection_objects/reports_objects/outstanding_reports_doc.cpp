
#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <outstanding_reports_keys.h>
#include <database_names.h>
#include <collection_names.h>

#include "reports_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserPictureDoc object to a document and saves it to the passed builder
void OutstandingReports::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::basic::array reports_log_arr;

    for (const ReportsLog& report_log : reports_log) {
        reports_log_arr.append(
                document{}
                        << outstanding_reports_keys::reports_log::ACCOUNT_OID << report_log.account_oid
                        << outstanding_reports_keys::reports_log::REASON << report_log.reason
                        << outstanding_reports_keys::reports_log::MESSAGE << report_log.message
                        << outstanding_reports_keys::reports_log::REPORT_ORIGIN << report_log.report_origin
                        << outstanding_reports_keys::reports_log::CHAT_ROOM_ID << report_log.chat_room_id
                        << outstanding_reports_keys::reports_log::MESSAGE_UUID << report_log.message_uuid
                        << outstanding_reports_keys::reports_log::TIMESTAMP_SUBMITTED << report_log.timestamp_submitted
                        << finalize
        );
    }

    if(checked_out_end_time_reached.value.count() != DEFAULT_DATE.value.count()) {
        document_result
            << outstanding_reports_keys::CHECKED_OUT_END_TIME_REACHED << checked_out_end_time_reached;
    }

    document_result
            << outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED << timestamp_limit_reached
            << outstanding_reports_keys::REPORTS_LOG << reports_log_arr;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

handle_reports::ReportedUserInfo OutstandingReports::convertToReportedUserInfo() const {
    handle_reports::ReportedUserInfo reported_user_info;

    reported_user_info.set_user_account_oid(current_object_oid.to_string());

    for(const auto& ele : reports_log) {
        auto* report_log = reported_user_info.add_reports_log();

        report_log->set_account_oid_of_report_sender(ele.account_oid.to_string());
        report_log->set_report_reason(ele.reason);
        report_log->set_message(ele.message);
        report_log->set_origin_type(ele.report_origin);
        report_log->set_chat_room_id(ele.chat_room_id);
        report_log->set_message_uuid(ele.message_uuid);
        report_log->set_timestamp_submitted(ele.timestamp_submitted.value.count());
    }

    return reported_user_info;
}

bool OutstandingReports::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            outstanding_reports_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = outstanding_reports_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool OutstandingReports::getFromCollection() {
    bsoncxx::builder::stream::document doc;
    return runGetFromCollection(doc);
}

bool OutstandingReports::getFromCollection(const bsoncxx::oid& findOID) {
    bsoncxx::builder::stream::document doc;
    doc
        << "_id" << findOID;

    return runGetFromCollection(doc);
}

bool OutstandingReports::runGetFromCollection(bsoncxx::builder::stream::document& find_doc) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = outstanding_reports_collection.find_one(find_doc.view());
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool OutstandingReports::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool OutstandingReports::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        timestamp_limit_reached = extractFromBsoncxx_k_date(
                user_account_document,
                outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED
        );

        auto checked_out_end_time_reached_element = user_account_document[outstanding_reports_keys::CHECKED_OUT_END_TIME_REACHED];
        if (checked_out_end_time_reached_element) { //if element exists and is type date
            checked_out_end_time_reached = checked_out_end_time_reached_element.get_date();
        } else { //if element does not exist or is not type date
            checked_out_end_time_reached = DEFAULT_DATE;
        }

        bsoncxx::array::view reports_log_arr = extractFromBsoncxx_k_array(
                user_account_document,
                outstanding_reports_keys::REPORTS_LOG
        );

        reports_log.clear();
        for (const auto& reports_log_doc : reports_log_arr) {
            bsoncxx::document::view reports_log_view = reports_log_doc.get_document().value;

            reports_log.emplace_back(
                    ReportsLog(
                            reports_log_view[outstanding_reports_keys::reports_log::ACCOUNT_OID].get_oid().value,
                            ReportReason(reports_log_view[outstanding_reports_keys::reports_log::REASON].get_int32().value),
                            reports_log_view[outstanding_reports_keys::reports_log::MESSAGE].get_string().value.to_string(),
                            ReportOriginType(reports_log_view[outstanding_reports_keys::reports_log::REPORT_ORIGIN].get_int32().value),
                            reports_log_view[outstanding_reports_keys::reports_log::CHAT_ROOM_ID].get_string().value.to_string(),
                            reports_log_view[outstanding_reports_keys::reports_log::MESSAGE_UUID].get_string().value.to_string(),
                            reports_log_view[outstanding_reports_keys::reports_log::TIMESTAMP_SUBMITTED].get_date()
                    )
            );
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool OutstandingReports::operator==(const OutstandingReports& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            timestamp_limit_reached.value.count(),
            other.timestamp_limit_reached.value.count(),
            "TIMESTAMP_LIMIT_REACHED",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            checked_out_end_time_reached.value.count(),
            other.checked_out_end_time_reached.value.count(),
            "CHECKED_OUT_END_TIME_REACHED",
            OBJECT_CLASS_NAME,
            return_value
            );

    if (other.reports_log.size() == reports_log.size()) {
        for (int i = 0; i < (int)reports_log.size(); i++) {
            checkForEquality(
                    reports_log[i].timestamp_submitted,
                    other.reports_log[i].timestamp_submitted,
                    "REPORTS_LOG.timestamp_submitted",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].message_uuid,
                    other.reports_log[i].message_uuid,
                    "REPORTS_LOG.message_uuid",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].chat_room_id,
                    other.reports_log[i].chat_room_id,
                    "REPORTS_LOG.chat_room_id",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].message,
                    other.reports_log[i].message,
                    "REPORTS_LOG.message",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].reason,
                    other.reports_log[i].reason,
                    "REPORTS_LOG.reason",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].account_oid.to_string(),
                    other.reports_log[i].account_oid.to_string(),
                    "REPORTS_LOG.reason",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    reports_log[i].report_origin,
                    other.reports_log[i].report_origin,
                    "REPORTS_LOG.report_origin",
                    OBJECT_CLASS_NAME,
                    return_value
                    );
        }
    } else {
        checkForEquality(
                reports_log.size(),
                other.reports_log.size(),
                "REPORTS_LOG.size()",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const OutstandingReports& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}

