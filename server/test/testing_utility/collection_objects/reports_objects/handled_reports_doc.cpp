
#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <outstanding_reports_keys.h>
#include <database_names.h>
#include <collection_names.h>
#include <handled_reports_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>

#include "reports_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserPictureDoc object to a document and saves it to the passed builder
void HandledReports::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::basic::array handled_reports_info_arr;

    for (const ReportsArrayElement& report_array_element : handled_reports_info) {

        bsoncxx::builder::basic::array reports_log_arr;

        for (const ReportsLog& report_log : report_array_element.reports_log) {
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

        bsoncxx::builder::stream::document handled_reports_doc;

        handled_reports_doc
            << handled_reports_keys::reports_array::ADMIN_NAME << report_array_element.admin_name
            << handled_reports_keys::reports_array::TIMESTAMP_HANDLED << report_array_element.timestamp_handled
            << handled_reports_keys::reports_array::TIMESTAMP_LIMIT_REACHED << report_array_element.timestamp_limit_reached
            << handled_reports_keys::reports_array::REPORT_HANDLED_MOVE_REASON << report_array_element.report_handled_move_reason
            << handled_reports_keys::reports_array::REPORTS_LOG << reports_log_arr;

        if(report_array_element.disciplinary_action_taken != DisciplinaryActionTypeEnum(-1)) {
            handled_reports_doc
                << handled_reports_keys::reports_array::DISCIPLINARY_ACTION_TAKEN << report_array_element.disciplinary_action_taken;
        }

        handled_reports_info_arr.append(handled_reports_doc);
    }

    document_result
            << handled_reports_keys::HANDLED_REPORTS_INFO << handled_reports_info_arr;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool HandledReports::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection handled_reports_collection = reports_db[collection_names::HANDLED_REPORTS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            handled_reports_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = handled_reports_collection.insert_one(insertDocument.view());

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

bool HandledReports::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection handled_reports_collection = reports_db[collection_names::HANDLED_REPORTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = handled_reports_collection.find_one(document{}
                                                                      << "_id" << findOID
                                                                      << finalize);
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

bool HandledReports::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool HandledReports::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        bsoncxx::array::view handled_reports_info_arr = extractFromBsoncxx_k_array(
                user_account_document,
                handled_reports_keys::HANDLED_REPORTS_INFO
        );

        for (const auto& handled_reports_info_doc : handled_reports_info_arr) {
            bsoncxx::document::view handled_reports_info_view = handled_reports_info_doc.get_document().value;

            bsoncxx::array::view reports_log_arr = extractFromBsoncxx_k_array(
                    handled_reports_info_view,
                    handled_reports_keys::reports_array::REPORTS_LOG
            );

            std::vector<ReportsLog> reports_log;
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

            auto disciplinary_action_taken = DisciplinaryActionTypeEnum(-1);

            auto disciplinary_action_taken_element =  handled_reports_info_view[handled_reports_keys::reports_array::DISCIPLINARY_ACTION_TAKEN];
            if(disciplinary_action_taken_element) {
                disciplinary_action_taken = DisciplinaryActionTypeEnum(disciplinary_action_taken_element.get_int32().value);
            }

            handled_reports_info.emplace_back(
                    ReportsArrayElement(
                            handled_reports_info_view[handled_reports_keys::reports_array::ADMIN_NAME].get_string().value.to_string(),
                            handled_reports_info_view[handled_reports_keys::reports_array::TIMESTAMP_HANDLED].get_date(),
                            handled_reports_info_view[handled_reports_keys::reports_array::TIMESTAMP_LIMIT_REACHED].get_date(),
                            ReportHandledMoveReason(handled_reports_info_view[handled_reports_keys::reports_array::REPORT_HANDLED_MOVE_REASON].get_int32().value),
                            disciplinary_action_taken,
                            reports_log
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

bool HandledReports::operator==(const HandledReports& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
            );

    if (other.handled_reports_info.size() == handled_reports_info.size()) {
        for (size_t i = 0; i < handled_reports_info.size(); i++) {

            checkForEquality(
                    handled_reports_info[i].admin_name,
                    other.handled_reports_info[i].admin_name,
                    "HANDLED_REPORTS_INFO.admin_name",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    handled_reports_info[i].timestamp_handled.value.count(),
                    other.handled_reports_info[i].timestamp_handled.value.count(),
                    "HANDLED_REPORTS_INFO.timestamp_handled",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    handled_reports_info[i].timestamp_limit_reached.value.count(),
                    other.handled_reports_info[i].timestamp_limit_reached.value.count(),
                    "HANDLED_REPORTS_INFO.timestamp_limit_reached",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    handled_reports_info[i].report_handled_move_reason,
                    other.handled_reports_info[i].report_handled_move_reason,
                    "HANDLED_REPORTS_INFO.report_handled_move_reason",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    handled_reports_info[i].disciplinary_action_taken,
                    other.handled_reports_info[i].disciplinary_action_taken,
                    "HANDLED_REPORTS_INFO.disciplinary_action_taken",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            const auto& reports_log = handled_reports_info[i].reports_log;
            const auto& other_reports_log = other.handled_reports_info[i].reports_log;

            if (other_reports_log.size() == reports_log.size()) {
                for (size_t j = 0; j < reports_log.size(); j++) {
                    checkForEquality(
                            reports_log[j].timestamp_submitted,
                            other_reports_log[j].timestamp_submitted,
                            "REPORTS_LOG.timestamp_submitted",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].message_uuid,
                            other_reports_log[j].message_uuid,
                            "REPORTS_LOG.message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].chat_room_id,
                            other_reports_log[j].chat_room_id,
                            "REPORTS_LOG.chat_room_id",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].message,
                            other_reports_log[j].message,
                            "REPORTS_LOG.message",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].reason,
                            other_reports_log[j].reason,
                            "REPORTS_LOG.reason",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].account_oid.to_string(),
                            other_reports_log[j].account_oid.to_string(),
                            "REPORTS_LOG.reason",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            reports_log[j].report_origin,
                            other_reports_log[j].report_origin,
                            "REPORTS_LOG.report_origin",
                            OBJECT_CLASS_NAME,
                            return_value
                            );
                }
            } else {
                checkForEquality(
                        reports_log.size(),
                        other_reports_log.size(),
                        "REPORTS_LOG.size()",
                        OBJECT_CLASS_NAME,
                        return_value
                        );
            }

        }
    } else {
        checkForEquality(
                handled_reports_info.size(),
                other.handled_reports_info.size(),
                "HANDLED_REPORTS_INFO.size()",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const HandledReports& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}