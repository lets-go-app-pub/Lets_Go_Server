#
# Dependencies
#
find_package(Threads REQUIRED)
find_package(TBB REQUIRED)
find_package(mongocxx 3.7.0 REQUIRED)
find_package(mongoc-1.0 1.22.1 REQUIRED)
find_package(Boost 1.75.0 COMPONENTS program_options filesystem REQUIRED)
find_package(PythonLibs 3.8 COMPONENTS Interpreter Development REQUIRED)

set(CMAKE_CXX_STANDARD 20)

# Only turn to TRUE when deploying to the remote server. This will disable all testing features,
set(setup_for_deployment TRUE)

if (NOT setup_for_deployment)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )

    FetchContent_MakeAvailable(googletest)
endif ()

set(SOURCES

        # --- Globals ---
        src/globals/global_bsoncxx_docs/global_bsoncxx_docs.h
        src/globals/global_bsoncxx_docs/global_bsoncxx_docs.cpp
        src/globals/number_chat_streams_running/number_chat_streams_running.h
        src/globals/mongodb_connection_pool/connection_pool_global_variable.h
        src/globals/server_accepting_connections_bool/server_accepting_connections_bool.h
        src/globals/thread_pool_variable/thread_pool_global_variable.h
        src/globals/user_open_chat_streams/user_open_chat_streams.h
        src/globals/python_modules/python_send_sms_module.h
        src/globals/python_modules/python_send_email_module.h
        src/globals/enums/report_handled_move_reason.h
        src/globals/server_values/server_values.h
        src/globals/assert_macro/assert_macro.h
        src/globals/file_IO_mutex_lock/file_IO_mutex_lock.h
        src/globals/admin_privileges_vector/admin_privileges_vector.h
        src/globals/file_IO_mutex_lock/file_IO_mutex_lock.h
        src/globals/enums/stored_type_enum.h
        src/globals/accepted_mine_types/accepted_mime_types.h
        src/globals/global_constants/database_names.h
        src/globals/global_constants/collection_names.h
        src/globals/global_constants/collection_field_names/chat_rooms_database/chat_room_shared_keys.h
        src/globals/global_constants/collection_field_names/chat_rooms_database/chat_room_info_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/icons_info_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/admin_account_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/user_account_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/pending_account_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/user_pictures_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/info_stored_after_deletion_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/account_recovery_keys.h
        src/globals/global_constants/collection_field_names/chat_rooms_database/chat_message_pictures_keys.h
        src/globals/global_constants/collection_field_names/deleted_database/deleted_user_pictures_keys.h
        src/globals/global_constants/collection_field_names/deleted_database/deleted_chat_message_pictures_keys.h
        src/globals/global_constants/collection_field_names/info_for_statistics_database/match_algorithm_results_keys.h
        src/globals/global_constants/collection_field_names/errors_database/fresh_errors_keys.h
        src/globals/global_constants/collection_field_names/errors_database/handled_errors_keys.h
        src/globals/global_constants/collection_field_names/errors_database/handled_errors_list_keys.h
        src/globals/global_constants/collection_field_names/info_for_statistics_database/match_algorithm_statistics_keys.h
        src/globals/global_constants/collection_field_names/info_for_statistics_database/individual_match_statistics_keys.h
        src/globals/global_constants/collection_field_names/info_for_statistics_database/user_account_statistics_documents_completed_keys.h
        src/globals/global_constants/collection_field_names/feedback_database/activity_suggestion_feedback_keys.h
        src/globals/global_constants/collection_field_names/feedback_database/other_suggestion_feedback_keys.h
        src/globals/global_constants/collection_field_names/feedback_database/bug_report_feedback_keys.h
        src/globals/global_constants/collection_field_names/reports_database/outstanding_reports_keys.h
        src/globals/global_constants/collection_field_names/reports_database/handled_reports_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/user_account_statistics_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/email_verification_keys.h
        src/globals/global_constants/algorithm_pipeline_field_names.h
        src/globals/global_constants/server_parameter_restrictions.h
        src/globals/global_constants/matching_algorithm.h
        src/globals/global_constants/chat_stream_container.h
        src/globals/global_constants/report_values.h
        src/globals/global_constants/feedback_values.h
        src/globals/global_constants/error_values.h
        src/globals/global_constants/request_statistics_values.h
        src/globals/global_constants/chat_room_values.h
        src/globals/global_constants/android_specific_values.h
        src/globals/global_constants/testing_specific_values.h
        src/globals/global_constants/general_values.h
        src/globals/global_constants/grpc_values.h
        src/globals/global_constants/mongodb_values.h
        src/globals/global_constants/version_number.h
        src/globals/global_constants/collection_field_names/chat_rooms_database/chat_room_header_keys.h
        src/globals/global_constants/collection_field_names/chat_rooms_database/chat_room_message_keys.h
        src/globals/global_constants/collection_field_names/accounts_database/activities_info_keys.h
        src/globals/global_constants/collection_field_names/deleted_database/deleted_accounts_keys.h
        src/globals/enums/reason_picture_deleted.h
        src/globals/global_constants/chat_change_stream_values.h
        src/globals/get_environment_variable/get_environment_variable.h
        src/globals/global_constants/collection_field_names/accounts_database/pre_login_checkers_keys.h
        src/globals/global_constants/event_admin_values.h

        # --- Utility ---
        src/utility/utility_general_functions/utility_general_functions.h
        src/utility/utility_general_functions/utility_general_functions.cpp
        src/utility/store_mongoDB_error_and_exception/store_mongoDB_error_and_exception.h
        src/utility/request_helper_functions/request_helper_functions.cpp
        src/utility/request_helper_functions/request_helper_functions.h
        src/utility/get_user_info_timestamps/get_user_info_timestamps.h
        src/utility/get_user_info_timestamps/get_user_info_timestamps.cpp
        src/utility/pre_login_timestamps/pre_login_timestamps.h
        src/utility/time_frame_struct/time_frame_struct.h
        src/utility/activity_struct/activity_struct.h
        src/utility/extract_thumbnail_from_verified_doc/extract_thumbnail_from_verified_doc.cpp
        src/utility/extract_thumbnail_from_verified_doc/extract_thumbnail_from_verified_doc.h
        src/utility/create_chat_room_helper/create_chat_room_helper.h
        src/utility/mongo_oid_concurrent_container/mongo_oid_concurrent_container.h
        src/utility/mongo_oid_concurrent_container/mongo_oid_concurrent_container.cpp
        src/utility/mongo_oid_concurrent_container/reference_wrapper.h
        src/utility/how_to_handle_member_pictures_enum/how_to_handle_member_pictures.h
        src/utility/utility_chat_functions/send_new_chat_room_and_messages.cpp
        src/utility/utility_chat_functions/utility_chat_functions.h
        src/utility/utility_chat_functions/save_user_info_to_member_shared_info_message.cpp
        src/utility/utility_chat_functions/convert_chat_message_document_to_chat_message_to_client.cpp
        src/utility/utility_chat_functions/get_user_account_info_for_user_joined_chat_room.cpp
        src/utility/utility_testing/utility_testing_functions.h
        src/utility/utility_testing/convert_message_instruction_to_string.cpp
        src/utility/utility_testing/convert_return_status_to_string.cpp
        src/utility/utility_testing/convert_success_types_to_string.cpp
        src/utility/utility_testing/convert_delete_type_to_string.cpp
        src/utility/server_initialization_functions/server_initialization_functions.h
        src/utility/server_initialization_functions/setup_mongo_db_indexing.cpp
        src/utility/server_initialization_functions/setup_mandatory_database_docs.cpp
        src/utility/async_server/async_server.h
        src/utility/utility_testing/convert_bool_to_string.cpp
        src/utility/utility_testing/convert_access_status_to_string.cpp
        src/utility/utility_testing/convert_login_functions_status_to_string.cpp
        src/utility/utility_testing/convert_sms_verification_status_to_string.cpp
        src/utility/utility_testing/convert_valid_array_element_return_enum_to_string.cpp
        src/utility/validate_login_status_functions/grpc_function_server_template.h
        src/utility/validate_login_status_functions/run_initial_login_operation.h
        src/utility/validate_login_status_functions/run_initial_login_operation.cpp
        src/utility/send_messages_implementation/send_messages_implementation.h
        src/utility/send_messages_implementation/insert_user_oid_to_chat_room_id.cpp
        src/utility/send_messages_implementation/erase_user_oid_from_chat_room_id.cpp
        src/utility/chat_stream_shared_info/chat_stream_concurrent_set.h
        src/utility/chat_stream_shared_info/chat_stream_concurrent_set.cpp
        src/utility/utility_chat_functions/is_valid_chat_room_id.cpp
        src/utility/store_and_send_messages/store_and_send_messages.h
        src/utility/store_and_send_messages/store_and_send_messages.cpp
        src/utility/build_validate_match_document/build_validate_match_document.h
        src/utility/build_validate_match_document/specific_match_queries/specific_match_queries.h
        src/utility/build_validate_match_document/specific_match_queries/match_account_is_activated.cpp
        src/utility/build_validate_match_document/specific_match_queries/match_age_inside_user_age_range.cpp
        src/utility/build_validate_match_document/specific_match_queries/user_age_inside_match_age_range.cpp
        src/utility/build_validate_match_document/specific_match_queries/user_gender_inside_match_gender_range.cpp
        src/utility/build_validate_match_document/specific_match_queries/match_gender_inside_user_gender_range.cpp
        src/utility/build_validate_match_document/specific_match_queries/activities_of_match_have_not_been_updated.cpp
        src/utility/build_validate_match_document/specific_match_queries/user_not_on_match_blocked_list.cpp
        src/utility/build_validate_match_document/specific_match_queries/user_not_recent_match_for_match.cpp
        src/utility/build_validate_match_document/specific_match_queries/distance_still_in_match_max_distance.cpp
        src/utility/build_validate_match_document/specific_match_queries/universal_match_conditions.cpp
        src/utility/build_validate_match_document/generate_pipeline_match_document.cpp
        src/utility/build_validate_match_document/generate_swiped_yes_on_user_match_document.cpp
        src/utility/build_validate_match_document/generate_swiped_yes_on_event_match_document.cpp
        src/utility/build_validate_match_document/generate_validate_array_element_match_document.cpp
        src/utility/create_chat_room_helper/generate_chat_room_id.cpp
        src/utility/create_chat_room_helper/create_chat_room_from_id.cpp
        src/utility/handle_function_operation_exception/handle_function_operation_exception.cpp
        src/utility/handle_function_operation_exception/handle_function_operation_exception.h
        src/utility/chat_stream_shared_info/call_data_command.h
        src/utility/utility_chat_functions/helper_functions/messages_to_client.cpp
        src/utility/utility_chat_functions/helper_functions/messages_to_client.h
        src/utility/utility_testing/convert_message_body_type_to_string.cpp
        src/utility/utility_chat_functions/extract_chat_picture.cpp
        src/utility/utility_testing/convert_pushed_to_queue_from_location_to_string.cpp
        src/utility/chat_stream_shared_info/pushed_from_queue_location_enum.h
        src/utility/utility_testing/convert_reply_body_case_to_string.cpp
        src/utility/utility_testing/convert_account_login_type_to_string.cpp
        src/utility/utility_testing/convert_server_response_case_to_string.cpp
        src/utility/utility_testing/convert_account_state_int_chat_room_to_string.cpp
        src/utility/utility_chat_functions/leave_chat_room.cpp
        src/utility/build_validate_match_document/specific_match_queries/at_least_one_category_matches.cpp
        src/utility/extract_data_from_bsoncxx/extract_data_from_bsoncxx.h
        src/utility/extract_data_from_bsoncxx/extract_data_from_bsoncxx.cpp
        src/utility/admin_functions_for_set_values/admin_functions_for_set_values.h
        src/utility/admin_functions_for_set_values/admin_functions_for_set_values.cpp
        src/utility/reports_helper_functions/save_new_report_to_collection.cpp
        src/utility/reports_helper_functions/report_helper_functions.h
        src/utility/reports_helper_functions/move_outstanding_reports_to_handled.cpp
        src/utility/admin_functions_for_request_values/admin_functions_for_request_values.h
        src/utility/admin_functions_for_request_values/extract_user_info_for_interface.cpp
        src/utility/admin_functions_for_request_values/extract_single_message_at_timestamp.cpp
        src/utility/empty_or_deleted_picture_info/deleted_thumbnail_info.h
        src/utility/async_server/async_server.cpp
        src/utility/store_mongoDB_error_and_exception/log_error_to_file.cpp
        src/utility/user_info_statistics_utility/store_info_to_user_statistics/store_info_to_user_statistics.cpp
        src/utility/user_info_statistics_utility/store_info_to_user_statistics/store_info_to_user_statistics.h
        src/utility/user_info_statistics_utility/move_user_account_statistics_document/move_user_account_statistics_document.cpp
        src/utility/user_info_statistics_utility/move_user_account_statistics_document/move_user_account_statistics_document.h
        src/utility/user_info_statistics_utility/build_user_statistics_document/build_user_statistics_document.cpp
        src/utility/user_info_statistics_utility/build_user_statistics_document/build_user_statistics_document.h
        src/utility/build_debug_string_response/build_debug_string_response.cpp
        src/utility/build_debug_string_response/build_debug_string_response.h
        src/utility/utility_testing/convert_different_user_joined_chat_room_amount.cpp
        src/utility/update_members_functions/update_single_other_user.cpp
        src/utility/utility_testing/duration_as_nano_seconds.cpp
        src/utility/update_members_functions/update_single_other_user.h
        src/utility/update_members_functions/build_update_single_other_user_projection_document.cpp
        src/utility/session_to_run_function/session_to_run_functions.h
        src/utility/session_to_run_function/count_documents_optional_session.cpp
        src/utility/session_to_run_function/create_index_optional_session.cpp
        src/utility/session_to_run_function/delete_many_optional_session.cpp
        src/utility/session_to_run_function/delete_one_optional_session.cpp
        src/utility/session_to_run_function/distinct_optional_session.cpp
        src/utility/session_to_run_function/find_optional_session.cpp
        src/utility/session_to_run_function/find_one_optional_session.cpp
        src/utility/session_to_run_function/find_one_and_delete_optional_session.cpp
        src/utility/session_to_run_function/find_one_and_replace_optional_session.cpp
        src/utility/session_to_run_function/find_one_and_update_optional_session.cpp
        src/utility/session_to_run_function/insert_one_optional_session.cpp
        src/utility/session_to_run_function/replace_one_optional_session.cpp
        src/utility/session_to_run_function/update_many_optional_session.cpp
        src/utility/session_to_run_function/update_one_optional_session.cpp
        src/utility/session_to_run_function/aggregate_optional_session.cpp
        src/utility/set_categories_helpers/sort_time_frames_and_remove_overlaps.cpp
        src/utility/set_categories_helpers/sort_time_frames_and_remove_overlaps.h
        src/utility/set_categories_helpers/save_activity_time_frames.cpp
        src/utility/set_categories_helpers/save_activity_time_frames.h
        src/utility/set_categories_helpers/save_category_time_frames.cpp
        src/utility/set_categories_helpers/save_category_time_frames.h
        src/utility/utility_testing/convert_call_data_command_types_to_string.cpp
        src/utility/utility_testing/grpc_mock_stream/mock_stream.h
        src/utility/thread_pool/coroutine_thread_pool.h
        src/utility/utility_testing/convert_bson_type_to_string.cpp
        src/utility/thread_pool/coroutine_type.h
        src/utility/thread_pool/coroutine_thread_pool.cpp
        src/utility/spin_lock/coroutine_spin_lock.h
        src/utility/chat_change_stream/chat_change_stream.h
        src/utility/chat_change_stream/chat_change_stream.cpp
        src/utility/chat_change_stream/messages_waiting_to_be_sent.h
        src/utility/chat_change_stream/chat_change_stream_helper_functions.h
        src/utility/chat_change_stream/chat_change_stream_helper_functions.cpp
        src/utility/utility_chat_functions/extract_thumbnail_from_header_account_document.cpp
        src/utility/empty_or_deleted_picture_info/deleted_picture_info.h
        src/utility/c_python_handle_gil_state/python_handle_gil_state.h
        src/utility/utility_find_matches_functions/find_matches_helper_objects.h
        src/utility/utility_find_matches_functions/utility_find_matches_functions.h
        src/utility/validate_login_status_functions/run_initial_login_operation.cpp
        src/utility/validate_login_status_functions/run_initial_login_operation.h
        src/utility/validate_login_status_functions/run_initial_login_with_match_parameter.h
        src/utility/validate_login_status_functions/get_login_document.h
        src/utility/empty_or_deleted_picture_info/empty_or_deleted_picture_info.h
        src/utility/create_chat_room_helper/generate_chat_room_cap_message.h
        src/utility/create_chat_room_helper/generate_chat_room_cap_message.cpp
        src/utility/create_user_account/create_user_account.cpp
        src/utility/create_user_account/create_user_account.h
        src/utility/build_validate_match_document/specific_match_queries/event_expiration_not_reached.cpp
        src/utility/join_chat_room_with_user/join_chat_room_with_user.cpp
        src/utility/join_chat_room_with_user/join_chat_room_with_user.h
        src/utility/create_chat_room_helper/create_complete_chat_room_with_user.cpp
        src/utility/create_chat_room_helper/create_complete_chat_room_with_user.h
        src/utility/create_chat_room_helper/generate_new_chat_room_times.h
        src/utility/create_chat_room_helper/full_event_values.h
        src/utility/create_chat_room_helper/create_chat_room_location_struct.h
        src/utility/event_helper_functions/event_request_message_is_valid.cpp
        src/utility/event_helper_functions/event_request_message_is_valid.h
        src/utility/event_helper_functions/create_event.cpp
        src/utility/event_helper_functions/create_event.h
        src/utility/create_chat_room_helper/create_initial_chat_room_messages.h
        src/utility/create_chat_room_helper/create_initial_chat_room_messages.cpp
        src/utility/server_initialization_functions/create_and_store_event_admin_account.cpp

        # --- Grpc Functions ---
        src/grpc_functions/login_function/login_function.h
        src/grpc_functions/login_function/login_function.cpp
        src/grpc_functions/sMS_verification/sMS_verification.h
        src/grpc_functions/sMS_verification/sMS_verification.cpp
        src/grpc_functions/login_support_functions/login_support_functions.h
        src/grpc_functions/login_support_functions/logout_function.cpp
        src/grpc_functions/login_support_functions/delete_account.cpp
        src/grpc_functions/login_support_functions/find_needed_verification_info.cpp
        src/grpc_functions/send_error_to_server/send_error_to_server.h
        src/grpc_functions/send_error_to_server/send_error_to_server.cpp
        src/grpc_functions/request_fields_functions/request_fields_functions.h
        src/grpc_functions/request_fields_functions/request_phone_number.cpp
        src/grpc_functions/request_fields_functions/request_birthday.cpp
        src/grpc_functions/request_fields_functions/request_email.cpp
        src/grpc_functions/request_fields_functions/request_gender.cpp
        src/grpc_functions/request_fields_functions/request_name.cpp
        src/grpc_functions/request_fields_functions/request_picture.cpp
        src/grpc_functions/request_fields_functions/request_categories.cpp
        src/grpc_functions/request_fields_functions/request_post_login_info.cpp
        src/grpc_functions/request_fields_functions/request_server_icons.cpp
        src/grpc_functions/request_fields_functions/request_server_activities.cpp
        src/grpc_functions/set_fields_functions/set_fields_functions.h
        src/grpc_functions/set_fields_functions/set_birthday.cpp
        src/grpc_functions/set_fields_functions/set_email.cpp
        src/grpc_functions/set_fields_functions/set_gender.cpp
        src/grpc_functions/set_fields_functions/set_first_name.cpp
        src/grpc_functions/set_fields_functions/set_picture.cpp
        src/grpc_functions/set_fields_functions/set_categories.cpp
        src/grpc_functions/set_fields_functions/set_age_range.cpp
        src/grpc_functions/set_fields_functions/set_gender_range.cpp
        src/grpc_functions/set_fields_functions/set_user_bio.cpp
        src/grpc_functions/set_fields_functions/set_user_city.cpp
        src/grpc_functions/set_fields_functions/set_max_distance.cpp
        src/grpc_functions/set_fields_functions/set_feedback.cpp
        src/grpc_functions/set_fields_functions/set_fields_helper_functions/set_fields_helper_functions.h
        src/grpc_functions/set_admin_fields/set_server_category_or_activity.cpp
        src/grpc_functions/set_admin_fields/set_admin_fields.h
        src/grpc_functions/set_admin_fields/set_server_icons.cpp
        src/grpc_functions/user_match_options/user_match_options.h
        src/grpc_functions/user_match_options/user_match_options.cpp
        src/grpc_functions/user_match_options/helper_functions/move_extracted_element_to_other_account.cpp
        src/grpc_functions/user_match_options/helper_functions/add_pull_array_to_query.cpp
        src/grpc_functions/user_match_options/helper_functions/add_filter_for_projection_stage.cpp
        src/grpc_functions/user_match_options/helper_functions/handle_user_extracted_account.cpp
        src/grpc_functions/user_match_options/helper_functions/user_match_options_helper_functions.h
        src/grpc_functions/user_match_options/helper_functions/update_match_statistics_document.cpp
        src/grpc_functions/find_matches/find_matches.h
        src/grpc_functions/find_matches/find_matches.cpp
        src/grpc_functions/find_matches/helper_functions/find_matches_helper_functions.h
        src/grpc_functions/find_matches/helper_functions/extract_user_info.cpp
        src/grpc_functions/find_matches/helper_functions/validate_array_element.cpp
        src/grpc_functions/find_matches/helper_functions/extract_and_save_matching_results.cpp
        src/grpc_functions/find_matches/helper_functions/organize_user_info_for_algorithm.cpp
        src/grpc_functions/find_matches/helper_functions/begin_algorithm_check_cooldown.cpp
        src/grpc_functions/find_matches/helper_functions/run_matching_algorithm.cpp
        src/grpc_functions/find_matches/helper_functions/extract_match_account_info_and_write_to_client.cpp
        src/grpc_functions/find_matches/helper_functions/save_find_matches_statistics.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_query_for_pipeline.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_activities_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_initialize_and_clean_time_frames_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_merge_time_frames_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_time_frame_time_statistics_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_calculate_final_point_intermediates_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_clean_up_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_sort_by_total_points_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_limit_pipeline_stage.cpp
        src/grpc_functions/chat_message_stream/chat_message_stream.h
        src/grpc_functions/chat_message_stream/check_login_token.cpp
        src/grpc_functions/chat_message_stream/extract_chat_rooms.cpp
        src/grpc_functions/chat_room_commands/chat_room_commands.h
        src/grpc_functions/chat_room_commands/client_message_to_server.cpp
        src/grpc_functions/chat_room_commands/helper_objects/chat_room_commands_helper_objects.h
        src/grpc_functions/chat_room_commands/helper_functions/chat_room_commands_helper_functions.h
        src/grpc_functions/chat_room_commands/helper_functions/send_message_to_chat_room.cpp
        src/grpc_functions/chat_room_commands/helper_functions/chat_room_message_is_able_to_reply.cpp
        src/grpc_functions/chat_room_commands/helper_functions/handle_message_to_be_sent.cpp
        src/grpc_functions/chat_room_commands/helper_functions/remove_message_for_error.cpp
        src/grpc_functions/chat_room_commands/create_chat_room.cpp
        src/grpc_functions/chat_room_commands/join_chat_room.cpp
        src/grpc_functions/chat_room_commands/leave_chat_room.cpp
        src/grpc_functions/chat_room_commands/remove_from_chat_room.cpp
        src/grpc_functions/chat_room_commands/un_match.cpp
        src/grpc_functions/chat_room_commands/block_and_report_chat_room.cpp
        src/grpc_functions/chat_room_commands/helper_functions/un_match_helper.cpp
        src/grpc_functions/chat_room_commands/un_block_other_user.cpp
        src/grpc_functions/chat_room_commands/promote_new_admin.cpp
        src/grpc_functions/chat_room_commands/update_chat_room_info.cpp
        src/grpc_functions/chat_room_commands/request_information/update_single_chat_room_member.cpp
        src/grpc_functions/chat_room_commands/request_information/update_chat_room.cpp
        src/grpc_functions/retrieve_server_load/retrieve_server_load.h
        src/grpc_functions/chat_message_stream/chat_stream_container_object.cpp
        src/grpc_functions/chat_message_stream/chat_stream_container_object.h
        src/grpc_functions/chat_message_stream/thread_safe_writes_waiting_to_process_vector.h
        src/grpc_functions/chat_room_commands/helper_functions/update_single_chat_room_member_not_in_chat_room.cpp
        src/grpc_functions/chat_room_commands/helper_functions/generate_different_user_joined_chat_room_message.cpp
        src/grpc_functions/chat_room_commands/helper_functions/send_different_user_joined_chat_room_delayed.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_categories_pipeline_stage.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_only_useful_fields.cpp
        src/grpc_functions/request_admin_info/request_admin_info.h
        src/grpc_functions/request_admin_info/initial_program_open_request_info.cpp
        src/grpc_functions/handle_feedback/handle_feedback.h
        src/grpc_functions/handle_feedback/handle_feedback_helper_functions/handle_feedback_helper_functions.h
        src/grpc_functions/handle_feedback/handle_feedback_helper_functions/save_feedback_info_to_vector.cpp
        src/grpc_functions/handle_feedback/handle_feedback_helper_functions/extract_and_save_feedback_to_response.cpp
        src/grpc_functions/handle_feedback/set_feedback_to_spam.cpp
        src/grpc_functions/request_user_account_info/request_user_account_info.h
        src/grpc_functions/set_admin_fields/set_server_access_status.cpp
        src/grpc_functions/set_admin_fields/remove_user_picture.cpp
        src/grpc_functions/handle_reports/time_out_user.cpp
        src/grpc_functions/handle_reports/dismiss_report.cpp
        src/grpc_functions/handle_reports/handle_reports.h
        src/grpc_functions/handle_reports/request_reports.cpp
        src/grpc_functions/admin_chat_room_commands/admin_chat_room_commands.h
        src/grpc_functions/admin_chat_room_commands/get_single_message.cpp
        src/grpc_functions/handle_reports/set_report_to_spam.cpp
        src/grpc_functions/manage_server_commands/manage_server_commands.h
        src/grpc_functions/manage_server_commands/request_server_shutdown.cpp
        src/grpc_functions/handle_errors/handle_errors.h
        src/grpc_functions/handle_errors/search_errors.cpp
        src/grpc_functions/handle_errors/extract_errors.cpp
        src/grpc_functions/handle_errors/delete_single_error.cpp
        src/grpc_functions/handle_errors/set_error_to_handled.cpp
        src/grpc_functions/handle_errors/helper_functions/handle_errors_helper_functions.h
        src/grpc_functions/handle_errors/helper_functions/error_parameters_are_good.cpp
        src/grpc_functions/handle_errors/helper_functions/merge_errors_to_handled.cpp
        src/grpc_functions/request_statistics/request_statistics.h
        src/grpc_functions/request_statistics/matching_activity_statistics.cpp
        src/grpc_functions/request_statistics/age_gender_statistics.cpp
        src/grpc_functions/handle_feedback/update_feedback_time_viewed.cpp
        src/grpc_functions/handle_feedback/get_initial_feedback.cpp
        src/grpc_functions/handle_feedback/get_feedback_update.cpp
        src/grpc_functions/handle_feedback/handle_feedback_helper_functions/verify_request_and_set_variables_feedback.cpp
        src/grpc_functions/request_user_account_info/request_user_account_info.cpp
        src/grpc_functions/find_matches/helper_functions/project_user_account_fields_out_for_statistics.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_final_points.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_distance_from_locations.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_categories_activities_matching.cpp
        src/grpc_functions/user_match_options/update_single_match_member.cpp
        src/grpc_functions/set_fields_functions/set_algorithm_search_options.cpp
        src/grpc_functions/chat_room_commands/helper_functions/client_message_to_server_helper_functions/client_message_to_server_helper_functions.cpp
        src/grpc_functions/chat_room_commands/helper_functions/client_message_to_server_helper_functions/client_message_to_server_helper_functions.h
        src/grpc_functions/chat_room_commands/helper_functions/update_user_last_viewed_time.cpp
        src/grpc_functions/chat_room_commands/helper_functions/filter_and_store_users_to_be_updated.cpp
        src/grpc_functions/set_fields_functions/set_fields_helper_functions/remove_invalid_elements_for_updated_matching_parameters.cpp
        src/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_conditional_switches_for_categories.cpp
        src/grpc_functions/find_matches/helper_functions/build_aggregation_pipeline.cpp
        src/grpc_functions/user_match_options/helper_functions/build_update_doc_for_response_type.h
        src/grpc_functions/user_match_options/helper_functions/check_for_no_elements_in_array.h
        src/grpc_functions/find_matches/helper_functions/run_matching_algorithm_and_return_results.cpp
        src/grpc_functions/user_match_options/helper_functions/handle_user_swiped_yes_on_event.cpp
        src/grpc_functions/chat_room_commands/set_pinned_location.cpp
        src/grpc_functions/admin_event_commands/admin_event_commands.h
        src/grpc_functions/admin_event_commands/add_admin_event.cpp
        src/grpc_functions/user_event_commands/user_event_commands.h
        src/grpc_functions/user_event_commands/add_user_event.cpp
        src/grpc_functions/user_event_commands/cancel_event.cpp

        # --- Testing Functions ---
        src/testing/generate_accounts/generate_multiple_accounts_multi_thread.cpp
        src/testing/generate_accounts/generate_multiple_accounts_multi_thread.h
        src/testing/generate_accounts/generate_multiple_random_accounts.cpp
        src/testing/generate_accounts/generate_multiple_random_accounts.h
        src/testing/generate_accounts/generate_random_account_info/generate_random_account_info.h
        src/testing/generate_accounts/generate_random_account_info/generate_random_account_info.cpp
        src/testing/generate_accounts/generate_random_account_info/generate_random_user_activities.cpp
        src/testing/generate_accounts/generate_random_account_info/generate_random_user_activities.h
        src/testing/generate_accounts/generate_and_store_complete_account/generate_and_store_complete_account.cpp
        src/testing/generate_accounts/generate_and_store_complete_account/generate_and_store_complete_account.h
        src/testing/generate_accounts/generate_manual_accounts.cpp
        src/testing/generate_accounts/generate_manual_accounts.h src/testing/send_pictures_for_testing/send_pictures_for_testing.h
        src/testing/send_pictures_for_testing/byte_array_storage.cpp
        src/testing/generate_accounts/generate_indexing_for_accounts.cpp
        src/testing/generate_accounts/generate_indexing_for_accounts.h
        src/grpc_functions/admin_event_commands/get_events.cpp src/grpc_functions/set_fields_functions/set_opted_in_to_promotional_email.cpp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

set(PYTHON_FILES
        src/grpc_functions/email_sending_messages/email_sending_messages.h
        src/utility/server_initialization_functions/setup_python_modules.cpp
        src/grpc_functions/email_sending_messages/helper_functions/helper_functions.h
        src/grpc_functions/email_sending_messages/helper_functions/attempt_to_generate_unique_code_twice.cpp
        src/grpc_functions/email_sending_messages/helper_functions/send_email_helper.cpp
        src/grpc_functions/email_sending_messages/email_verification.cpp
        src/grpc_functions/email_sending_messages/account_recovery.cpp
        python/send_sms.py
        python/send_email.py
        )

if (setup_for_deployment)
    # See below for more details
    add_compile_definitions(
            _RELEASE

            #Used to allow the async server to accept EmailSendingMessagesImpl. Used for testing.
            _ACCEPT_SEND_EMAIL_COMMANDS
            _ACCEPT_SEND_SMS_COMMANDS
    )
else ()
    add_compile_definitions(

            # This will print logs of the chat stream. Note that it will slow down the process by quite a bit.
            # CHAT_STREAM_LOG

            _DEBUG
            #_RELEASE

            #Used to allow the async server to accept EmailSendingMessagesImpl. Used for testing.
            # _ACCEPT_SEND_EMAIL_COMMANDS
            # _ACCEPT_SEND_SMS_COMMANDS

            #A flag that should be turned on when running any kind of tests and off otherwise. Changes some global
            # variables
            #NOTE: Not mutually exclusive with _DEBUG, however should be 'off' for _RELEASE.
            # LG_TESTING
    )
endif ()

#set(GOOGLETEST_VERSION 1.11.0)
#add_subdirectory(test/googletest)

if (NOT setup_for_deployment)

    set(TESTING_FILES
            test/utility/utility_general_functions/testing.cpp
            test/testing_utility/clear_database_for_testing.cpp
            test/testing_utility/clear_database_for_testing.h
            test/testing_utility/collection_objects/accounts_objects/account_objects.h
            test/testing_utility/collection_objects/accounts_objects/user_account_doc.cpp
            test/testing_utility/collection_objects/base_collection_object.h
            test/testing_utility/collection_objects/accounts_objects/user_picture_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/user_account_statistics_doc.cpp
            test/utility/user_info_statistics_utility/move_user_account_statistics_document/testing.cpp
            test/testing_utility/collection_objects/info_for_statistics_objects/info_for_statistics_objects.h
            test/testing_utility/collection_objects/info_for_statistics_objects/user_account_statistics_documents_completed_doc.cpp
            test/utility/user_info_statistics_utility/move_user_account_statistics_document/move_user_account_statistics_document_test.h
            test/utility/user_info_statistics_utility/move_user_account_statistics_document/move_user_account_statistics_document_test.cpp
            test/utility/reports_helper_functions/testing.cpp
            test/utility/reports_helper_functions/report_helper_functions_test.h
            test/testing_utility/collection_objects/reports_objects/reports_objects.h
            test/testing_utility/collection_objects/reports_objects/handled_reports_doc.cpp
            test/testing_utility/collection_objects/reports_objects/outstanding_reports_doc.cpp
            test/testing_utility/generate_randoms/generate_randoms.h
            test/testing_utility/generate_randoms/generate_random_chat_room_id.cpp
            test/testing_utility/generate_randoms/generate_random_reports_log.cpp
            test/testing_utility/generate_randoms/generate_random_outstanding_reports.cpp
            test/utility/reports_helper_functions/move_outstanding_reports_to_handled.cpp
            test/testing_utility/collection_objects/deleted_objects/deleted_objects.h
            test/testing_utility/collection_objects/deleted_objects/deleted_user_pictures_docs.cpp
            test/testing_utility/check_function_results/utility_general_functions/utility_general_functions_test.h
            test/testing_utility/generate_randoms/generate_random_user_picture.cpp
            test/testing_utility/check_function_results/utility_general_functions/delete_picture_document.cpp
            test/testing_utility/check_function_results/utility_general_functions/delete_account_pictures.cpp
            test/utility/utility_chat_functions/testing.cpp
            test/testing_utility/collection_objects/chat_rooms_objects/chat_message_pictures_doc.cpp
            test/testing_utility/collection_objects/chat_rooms_objects/chat_room_header_doc.cpp
            test/testing_utility/collection_objects/chat_rooms_objects/chat_room_info_doc.cpp
            test/testing_utility/collection_objects/chat_rooms_objects/chat_room_message_doc.cpp
            test/testing_utility/collection_objects/chat_rooms_objects/chat_rooms_objects.h
            test/testing_utility/check_function_results/utility_chat_functions/utility_chat_functions_test.h
            test/testing_utility/check_function_results/utility_chat_functions/leave_chat_room.cpp
            test/testing_utility/setup_login_info/setup_user_login_info.cpp
            test/testing_utility/setup_login_info/setup_login_info.h
            test/testing_utility/collection_objects/deleted_objects/deleted_accounts_docs.cpp
            test/testing_utility/collection_objects/deleted_objects/deleted_chat_message_pictures_docs.cpp
            test/testing_utility/collection_objects/accounts_objects/account_recovery_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/activities_info_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/admin_account_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/email_verification_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/icons_info_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/info_stored_after_deletion_doc.cpp
            test/testing_utility/collection_objects/accounts_objects/pending_account_doc.cpp
            test/testing_utility/collection_objects/errors_objects/errors_objects.h
            test/testing_utility/async_server_utility/start_server.h
            test/testing_utility/async_server_utility/start_server.cpp
            test/testing_utility/collection_objects/errors_objects/fresh_errors_docs.cpp
            test/testing_utility/collection_objects/errors_objects/handled_errors_docs.cpp
            test/testing_utility/collection_objects/errors_objects/handled_errors_list_docs.cpp
            test/testing_utility/collection_objects/feedback_objects/feedback_objects.h
            test/testing_utility/collection_objects/feedback_objects/activity_suggestion_feedback_docs.cpp
            test/testing_utility/collection_objects/feedback_objects/bug_report_feedback_docs.cpp
            test/testing_utility/collection_objects/feedback_objects/other_suggestion_feedback_docs.cpp
            test/testing_utility/collection_objects/info_for_statistics_objects/individual_match_statistics_docs.cpp
            test/testing_utility/collection_objects/info_for_statistics_objects/match_algorithm_results_docs.cpp
            test/testing_utility/collection_objects/info_for_statistics_objects/match_algorithm_statistics_docs.cpp
            test/testing_utility/setup_login_info/setup_admin_login_info.cpp
            test/utility/admin_functions_for_request_values/testing.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_text_message.cpp
            test/utility/admin_functions_for_set_values/testing.cpp
            test/utility/build_debug_string_response/testing.cpp
            test/utility/build_validate_match_document/testing.cpp
            test/utility/build_validate_match_document/specific_match_queries/testing.cpp
            test/utility/chat_stream_shared_info/testing.cpp
            test/utility/create_chat_room_helper/testing.cpp
            test/utility/extract_data_from_bsoncxx/testing.cpp
            test/utility/extract_thumbnail_from_verified_doc/testing.cpp
            test/utility/get_user_info_timestamps/testing.cpp
            test/utility/validate_login_status_functions/grpc_function_server_template_testing.cpp
            test/utility/handle_function_operation_exception/testing.cpp
            test/utility/mongo_oid_concurrent_container/testing.cpp
            test/utility/request_helper_functions/testing.cpp
            test/utility/send_messages_implementation/testing.cpp
            test/utility/server_initialization_functions/testing.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_picture_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_location_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_mime_type_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_invite_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_deleted_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_edited_message.cpp
            test/testing_utility/generate_randoms/generate_random_user_activity_detected_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_messages.h
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_update_observed_time_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_user_kicked_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_user_banned_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_chat_room_name_updated_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_chat_room_password_updated_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_new_admin_promoted_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_match_canceled_message.cpp
            test/testing_utility/generate_randoms/generate_random_messages/generate_random_user_activity_detected_message.cpp
            test/utility/session_to_run_function/testing.cpp
            test/utility/set_categories_helpers/testing.cpp
            test/utility/store_and_send_messages/testing.cpp
            test/utility/store_mongoDB_error_and_exception/testing.cpp
            test/utility/update_members_functions/testing.cpp
            test/utility/user_info_statistics_utility/build_user_statistics_document/testing.cpp
            test/utility/user_info_statistics_utility/store_info_to_user_statistics/testing.cpp
            test/testing_utility/generate_randoms/generate_random_chat_room_picture.cpp
            test/utility/utility_chat_functions/helper_functions/testing.cpp
            test/utility/utility_testing/testing.cpp
            test/utility/async_server/testing.cpp
            test/testing_utility/async_server_utility/testing_client.h
            test/testing_utility/async_server_utility/start_bi_di_stream.cpp
            test/testing_utility/async_server_utility/start_bi_di_stream.h
            test/utility/async_server/small_stress_testing.cpp
            test/utility/spin_lock/testing.cpp
            test/utility/thread_pool/testing.cpp
            test/grpc_functions/chat_message_stream/testing_chat_stream_container_object.cpp
            test/grpc_functions/chat_message_stream/testing.cpp
            test/grpc_functions/admin_chat_room_commands/testing.cpp
            test/testing_utility/generate_temp_admin_account/generate_temp_admin_account.cpp
            test/testing_utility/generate_temp_admin_account/generate_temp_admin_account.h
            test/grpc_functions/chat_room_commands/helper_functions/testing.cpp
            test/testing_utility/build_match_made_chat_room/build_match_made_chat_room.h
            test/testing_utility/build_match_made_chat_room/build_match_made_chat_room.cpp
            test/grpc_functions/chat_room_commands/block_and_report_chat_room_testing.cpp
            test/grpc_functions/chat_room_commands/client_message_to_server_testing.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_message_to_server.h
            test/testing_utility/setup_client_message_to_server/setup_client_text_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_picture_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_mime_type_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_invite_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_edited_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_deleted_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_observed_time_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_user_activity_detected_message_request.cpp
            test/testing_utility/setup_client_message_to_server/setup_client_location_message_request.cpp
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_reply_to_request.h
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_text_reply_to_request.cpp
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_picture_reply_to_request.cpp
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_location_reply_to_request.cpp
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_invite_reply_to_request.cpp
            test/testing_utility/setup_client_message_to_server/add_reply_to_request/add_mime_type_reply_to_request.cpp
            test/testing_utility/collection_objects/accounts_objects/matching_element.cpp
            test/testing_utility/check_function_results/utility_general_functions/build_and_check_delete_account.cpp
            test/testing_utility/various/save_activities_and_categories.cpp
            test/testing_utility/various/save_activities_and_categories.h
            test/testing_utility/setup_login_info/check_login_info_admin_and_client.cpp
            test/testing_utility/various/compare_equivalent_messages.h
            test/grpc_functions/chat_room_commands/create_chat_room_testing.cpp
            test/grpc_functions/chat_room_commands/join_chat_room_testing.cpp
            test/testing_utility/change_stream_utility/change_stream_utility.cpp
            test/testing_utility/change_stream_utility/change_stream_utility.h
            test/testing_utility/generate_randoms/generate_random_largest_possible_user_account.cpp
            test/utility/chat_change_stream/chat_change_stream_testing.cpp
            test/utility/server_initialization_functions/testing.cpp
            test/utility/chat_change_stream/chat_change_stream_helper_functions_testing.cpp
            test/testing_utility/setup_login_info/check_login_info_client_only.cpp
            test/testing_utility/setup_login_info/check_login_info_admin_only.cpp
            test/grpc_functions/chat_room_commands/leave_chat_room_testing.cpp
            test/grpc_functions/chat_room_commands/promote_new_admin_testing.cpp
            test/grpc_functions/chat_room_commands/remove_from_chat_room_testing.cpp
            test/grpc_functions/chat_room_commands/un_block_other_user_testing.cpp
            test/grpc_functions/chat_room_commands/un_match_testing.cpp
            test/testing_utility/build_match_made_chat_room/check_if_unmatch_was_successful.cpp
            test/testing_utility/build_match_made_chat_room/setup_testing_for_un_match.cpp
            test/grpc_functions/chat_room_commands/request_information/update_chat_room_testing.cpp
            test/grpc_functions/chat_room_commands/update_chat_room_info_testing.cpp
            test/grpc_functions/chat_room_commands/request_information/update_single_chat_room_member_testing.cpp
            test/grpc_functions/chat_room_commands/request_information/request_information_helper_functions.cpp
            test/grpc_functions/chat_room_commands/request_information/request_information_helper_functions.h
            test/grpc_functions/email_sending_messages/helper_functions/testing.cpp
            test/grpc_functions/email_sending_messages/account_recovery_testing.cpp
            test/grpc_functions/email_sending_messages/email_verification_testing.cpp
            test/grpc_functions/email_sending_messages/helper_functions/send_email_testing.h
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_query_for_pipeline_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_categories_activities_matching_testing.cpp
            test/grpc_functions/find_matches/helper_functions/generate_matching_users.h
            test/grpc_functions/find_matches/helper_functions/generate_matching_users.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_activities_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_categories_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_initialize_and_clean_time_frames_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_merge_time_frames_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_add_fields_time_frame_time_statistics_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_project_calculate_final_point_intermediates_pipeline_stage_testing.cpp
            test/grpc_functions/find_matches/helper_functions/algorithm_pipeline_helper_functions/generate_distance_from_locations_testing.cpp
            test/grpc_functions/find_matches/helper_functions/run_matching_algorithm_testing.cpp
            test/grpc_functions/find_matches/helper_functions/organize_user_info_for_algorithm_testing.cpp
            test/grpc_functions/find_matches/helper_functions/extract_and_save_matching_results_testing.cpp
            test/grpc_functions/find_matches/helper_functions/begin_algorithm_check_cooldown_testing.cpp
            test/grpc_functions/find_matches/helper_functions/extract_match_account_info_and_write_to_client_testing.cpp
            test/grpc_functions/find_matches/helper_functions/extract_user_info_testing.cpp
            test/grpc_functions/find_matches/helper_functions/save_find_matches_statistics_testing.cpp
            test/grpc_functions/find_matches/helper_functions/validate_array_element_testing.cpp
            test/grpc_functions/find_matches/find_matches_testing.cpp
            test/grpc_functions/user_match_options/user_match_options_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/build_update_doc_for_response_type_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/add_pull_array_to_query_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/check_for_no_elements_in_array_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/update_match_statistics_document_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/handle_user_extracted_account_testing.cpp
            test/grpc_functions/user_match_options/helper_functions/move_extracted_element_to_other_account_testing.cpp
            test/utility/validate_login_status_functions/run_initial_login_operation_testing.cpp
            test/utility/validate_login_status_functions/run_initial_login_with_match_parameter_testing.cpp
            test/grpc_functions/set_fields_functions/set_fields_helper_functions/set_fields_helper_functions_testing.cpp
            test/utility/validate_login_status_functions/get_login_document_testing.cpp
            test/grpc_functions/user_match_options/update_single_match_member_testing.cpp
            test/grpc_functions/handle_errors/delete_single_error_testing.cpp
            test/grpc_functions/handle_errors/extract_errors_testing.cpp
            test/grpc_functions/handle_errors/search_errors_testing.cpp
            test/grpc_functions/handle_errors/set_error_to_handled_testing.cpp
            test/grpc_functions/handle_feedback/get_feedback_update_testing.cpp
            test/grpc_functions/handle_feedback/get_initial_feedback_testing.cpp
            test/grpc_functions/handle_feedback/set_feedback_to_spam_testing.cpp
            test/grpc_functions/handle_feedback/update_feedback_time_viewed_testing.cpp
            test/grpc_functions/handle_feedback/feedback_testing_helpers.h
            test/grpc_functions/handle_reports/dismiss_report_testing.cpp
            test/grpc_functions/handle_reports/request_reports_testing.cpp
            test/grpc_functions/handle_reports/set_report_to_spam_testing.cpp
            test/grpc_functions/handle_reports/time_out_user_testing.cpp
            test/grpc_functions/login_function/login_function_testing.cpp
            test/grpc_functions/login_support_functions/delete_account_testing.cpp
            test/grpc_functions/login_support_functions/find_needed_verification_info_testing.cpp
            test/grpc_functions/login_support_functions/logout_function_testing.cpp
            test/grpc_functions/manage_server_commands/request_server_shutdown_testing.cpp
            test/grpc_functions/request_admin_info/initial_program_open_request_info_testing.cpp
            test/grpc_functions/request_fields_functions/request_birthday_testing.cpp
            test/grpc_functions/request_fields_functions/request_categories_testing.cpp
            test/grpc_functions/request_fields_functions/request_email_testing.cpp
            test/grpc_functions/request_fields_functions/request_gender_testing.cpp
            test/grpc_functions/request_fields_functions/request_name_testing.cpp
            test/grpc_functions/request_fields_functions/request_phone_number_testing.cpp
            test/grpc_functions/request_fields_functions/request_pictures_testing.cpp
            test/grpc_functions/request_fields_functions/request_post_login_info_testing.cpp
            test/grpc_functions/request_fields_functions/request_server_activities_testing.cpp
            test/grpc_functions/request_fields_functions/request_server_icons_testing.cpp
            test/grpc_functions/request_statistics/age_gender_statistics_testing.cpp
            test/grpc_functions/request_statistics/matching_activity_statistics_testing.cpp
            test/grpc_functions/request_user_account_info/request_user_account_info_testing.cpp
            test/grpc_functions/retrieve_server_load/retrieve_server_load_testing.cpp
            test/grpc_functions/send_error_to_server/send_error_to_server_testing.cpp
            test/grpc_functions/set_admin_fields/remove_user_picture_testing.cpp
            test/grpc_functions/set_admin_fields/set_server_access_status_testing.cpp
            test/grpc_functions/set_admin_fields/set_server_category_or_activity_testing.cpp
            test/grpc_functions/set_admin_fields/set_server_icons_testing.cpp
            test/grpc_functions/set_fields_functions/set_age_range_testing.cpp
            test/grpc_functions/set_fields_functions/set_algorithm_search_options_testing.cpp
            test/grpc_functions/set_fields_functions/set_birthday_testing.cpp
            test/grpc_functions/set_fields_functions/set_categories_testing.cpp
            test/grpc_functions/set_fields_functions/set_email_testing.cpp
            test/grpc_functions/set_fields_functions/set_feedback_testing.cpp
            test/grpc_functions/set_fields_functions/set_first_name_testing.cpp
            test/grpc_functions/set_fields_functions/set_gender_testing.cpp
            test/grpc_functions/set_fields_functions/set_gender_range_testing.cpp
            test/grpc_functions/set_fields_functions/set_max_distance_testing.cpp
            test/grpc_functions/set_fields_functions/set_picture_testing.cpp
            test/grpc_functions/set_fields_functions/set_user_bio_testing.cpp
            test/grpc_functions/set_fields_functions/set_user_city_testing.cpp
            test/grpc_functions/sMS_verification/sMS_verification_testing.cpp
            test/globals/global_bsoncxx_docs/global_bsoncxx_docs_testing.cpp
            src/globals/get_environment_variable/get_environment_variable.h
            test/testing_utility/generate_randoms/generate_random_event.cpp test/testing_utility/generate_randoms/generate_random_event_request_message.cpp test/testing_utility/generate_randoms/generate_random_event_and_event_info.cpp test/grpc_functions/user_match_options/helper_functions/handle_user_swiped_yes_on_event_testing.cpp test/testing_utility/generate_randoms/generate_random_messages/generate_random_new_pinned_location_message.cpp test/grpc_functions/chat_room_commands/set_pinned_location_testing.cpp test/grpc_functions/user_event_commands/add_user_event_testing.cpp test/grpc_functions/user_event_commands/cancel_event_testing.cpp test/testing_utility/various/check_for_valid_event_request_message.cpp test/testing_utility/various/check_for_valid_event_request_message.h test/grpc_functions/admin_event_commands/add_admin_event_testing.cpp test/testing_utility/collection_objects/accounts_objects/pre_login_checkers_doc.cpp test/testing_utility/generate_randoms/generate_random_valid_user_account_type.cpp test/grpc_functions/admin_event_commands/get_events_testing.cpp test/grpc_functions/set_fields_functions/set_opted_in_to_promotional_email_testing.cpp)

    add_executable(tests test/test_entry_point.cpp
            ${SOURCES}
            ${PYTHON_FILES}
            ${TESTING_FILES}
            )

endif ()

#
# Target
#
add_executable(server src/lets_go_server_entry_point.cpp
        ${SOURCES}
        ${PYTHON_FILES}
        )

if (MSVC)
    target_compile_options(server PRIVATE /O0 Wall /WX)
    target_compile_options(tests PRIVATE /O0 /Wall /WX)
else ()

    if (setup_for_deployment)
        #Production setup
        target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -pthread -O3)
    else ()
        #Debug setup
        target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -g3 -pthread -O0)
        target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic -g3 -pthread -O0)
    endif ()

endif ()

include_directories(

        #"../cmake-build-debug/server_specific_generated_proto"
        #"../cmake-build-debug/client_server_generated_proto"

        # --- Globals ---
        "src/globals/admin_privileges_vector"
        "src/globals/user_open_chat_streams"
        "src/globals/thread_pool_variable"
        "src/globals/server_accepting_connections_bool"
        "src/globals/mongodb_connection_pool"
        "src/globals/number_chat_streams_running"
        "src/globals/enums"
        "src/globals/assert_macro"
        "src/globals/file_IO_mutex_lock"
        "src/globals/accepted_mine_types"
        "src/globals/server_values"
        "src/globals/python_modules"
        "src/globals/mongodb_connection_pool"
        "src/globals/global_constants"
        "src/globals/global_constants/collection_field_names"
        "src/globals/global_constants/collection_field_names/accounts_database"
        "src/globals/global_constants/collection_field_names/chat_rooms_database"
        "src/globals/global_constants/collection_field_names/deleted_database"
        "src/globals/global_constants/collection_field_names/errors_database"
        "src/globals/global_constants/collection_field_names/feedback_database"
        "src/globals/global_constants/collection_field_names/info_for_statistics_database"
        "src/globals/global_constants/collection_field_names/reports_database"
        "src/globals/global_bsoncxx_docs"
        "src/globals/get_environment_variable"

        # --- Utility ---
        "src/utility/store_mongoDB_error_and_exception"
        "src/utility/utility_general_functions"
        "src/utility/request_helper_functions"
        "src/utility/check_login_token_status"
        "src/utility/pre_login_timestamps"
        "src/utility/get_user_info_timestamps"
        "src/utility/time_frame_struct"
        "src/utility/activity_struct"
        "src/utility/extract_thumbnail_from_verified_doc"
        "src/utility/create_chat_room_helper"
        "src/utility/mongo_oid_concurrent_container"
        "src/utility/how_to_handle_member_pictures_enum"
        "src/utility/utility_chat_functions"
        "src/utility/utility_testing"
        "src/utility/server_initialization_functions"
        "src/utility/chat_change_stream"
        "src/utility/user_info_statistics_utility/move_user_account_statistics_document"
        "src/utility/user_info_statistics_utility/store_info_to_user_statistics"
        "src/utility/user_info_statistics_utility/build_user_statistics_document"
        "src/utility/validate_login_status_functions"
        "src/utility/chat_stream_shared_info"
        "src/utility/send_messages_implementation"
        "src/utility/store_and_send_messages"
        "src/utility/build_validate_match_document"
        "src/utility/handle_function_operation_exception"
        "src/globals/number_chat_streams_running"
        "src/utility/thread_pool"
        "src/utility/extract_data_from_bsoncxx"
        "src/utility/admin_functions_for_set_values"
        "src/utility/reports_helper_functions"
        "src/utility/TODO_exception"
        "src/utility/admin_functions_for_request_values"
        "src/utility/empty_or_deleted_picture_info"
        "src/utility/build_debug_string_response"
        "src/utility/async_server"
        "src/utility/update_members_functions"
        "src/utility/session_to_run_function"
        "src/utility/set_categories_helpers"
        "src/utility/utility_testing"
        "src/utility/spin_lock"
        "src/utility/c_python_handle_gil_state"
        "src/utility/utility_find_matches_functions"
        "src/utility/create_user_account"
        "src/utility/join_chat_room_with_user"
        "src/utility/event_helper_functions"

        # --- Grpc Function ---
        "src/grpc_functions/login_function"
        "src/grpc_functions/sMS_verification"
        "src/grpc_functions/login_support_functions"
        "src/grpc_functions/send_error_to_server"
        "src/grpc_functions/request_fields_functions"
        "src/grpc_functions/set_fields_functions"
        "src/grpc_functions/set_admin_fields"
        "src/grpc_functions/user_match_options"
        "src/grpc_functions/find_matches"
        "src/grpc_functions/chat_room_commands"
        "src/grpc_functions/chat_room_commands/helper_functions"
        "src/grpc_functions/chat_message_stream"
        "src/grpc_functions/email_sending_messages"
        "src/grpc_functions/request_admin_info"
        "src/grpc_functions/request_statistics"
        "src/grpc_functions/handle_feedback"
        "src/grpc_functions/request_user_account_info"
        "src/grpc_functions/handle_reports"
        "src/grpc_functions/admin_chat_room_commands"
        "src/grpc_functions/manage_server_commands"
        "src/grpc_functions/retrieve_server_load"
        "src/grpc_functions/handle_errors"
        "src/grpc_functions/admin_event_commands"
        "src/grpc_functions/user_event_commands"

        # --- testing ---
        "src/testing/generate_accounts"
        "src/testing/send_pictures_for_testing"

        ${Boost_INCLUDE_DIR}
        ${PYTHON_INCLUDE_DIRS}

)

if (NOT setup_for_deployment)
    include_directories(
            # --- test ---
            "test/testing_utility"
            "test/testing_utility/account_objects"
            "test/testing_utility/collection_objects"
            "test/testing_utility/collection_objects/accounts_objects"
            "test/testing_utility/collection_objects/chat_rooms_objects"
            "test/testing_utility/collection_objects/deleted_objects"
            "test/testing_utility/collection_objects/errors_objects"
            "test/testing_utility/collection_objects/feedback_objects"
            "test/testing_utility/collection_objects/info_for_statistics_objects"
            "test/testing_utility/collection_objects/reports_objects"
            "test/testing_utility/generate_randoms"
            "test/testing_utility/various"
            "test/testing_utility/generate_randoms/generate_random_messages"
            "test/testing_utility/setup_login_info"
            "test/testing_utility/change_stream_utility"
            "test/testing_utility/check_function_results/utility_general_functions"
            "test/testing_utility/check_function_results/utility_chat_functions"
            "test/utility/reports_helper_functions"
            "test/utility/user_info_statistics_utility/move_user_account_statistics_document"
            "test/utility/utility_chat_functions"
            "test/testing_utility/async_server_utility"
            "test/testing_utility/build_match_made_chat_room"
            "test/testing_utility/setup_client_message_to_server"
            "test/testing_utility/setup_client_message_to_server/add_reply_to_request"

            #        # -- Google Testing
            #        "test/googletest/include"
            #        "test/googletest"
    )
endif ()

target_link_libraries(server
        TBB::tbb
        mongo::mongocxx_shared
        mongo::mongoc_shared
        generated_proto_client_server_shared
        generated_proto_server_specific
        ${Boost_LIBRARIES}
        ${PYTHON_LIBRARIES}
        pthread
        dl
        )

if (NOT setup_for_deployment)
    target_link_libraries(tests
            TBB::tbb
            mongo::mongocxx_shared
            mongo::mongoc_shared
            generated_proto_client_server_shared
            generated_proto_server_specific
            ${Boost_LIBRARIES}
            ${PYTHON_LIBRARIES}
            pthread
            dl
            gtest
            gtest_main
            )
endif ()
