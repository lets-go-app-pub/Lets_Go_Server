//
// Created by jeremiah on 2/1/22.
//

#include <bsoncxx/builder/basic/array.hpp>

#include "algorithm_pipeline_helper_functions.h"

#include "user_account_keys.h"
#include "matching_algorithm.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bsoncxx::document::value generateDistanceFromLocations(
        const double& user_longitude,
        const double& user_latitude
        ) {
        return document{}
            << "$let" << open_document
                << "vars" << open_document
                    << "phi_two" << open_document
                        << "$degreesToRadians" << open_document
                            << "$arrayElemAt" << open_array
                                << '$' + user_account_keys::LOCATION + ".coordinates"
                                << 1
                            << close_array
                        << close_document
                    << close_document
                    << "delta_phi" << open_document
                        << "$degreesToRadians" << open_document
                            << "$subtract" << open_array
                                << open_document
                                    << "$arrayElemAt" << open_array
                                        << '$' + user_account_keys::LOCATION + ".coordinates"
                                        << 1
                                    << close_array
                                << close_document

                                << user_latitude
                            << close_array
                        << close_document
                    << close_document
                    << "delta_lambda" << open_document
                        << "$degreesToRadians" << open_document
                            << "$subtract" << open_array
                                << open_document
                                    << "$arrayElemAt" << open_array
                                        << '$' + user_account_keys::LOCATION + ".coordinates"
                                        << 0
                                    << close_array
                                << close_document

                                << bsoncxx::types::b_double{ user_longitude }
                            << close_array
                        << close_document
                    << close_document
                << close_document
                << "in" << open_document
                    << "$let" << open_document
                        << "vars" << open_document
                            << "sin_phi" << open_document
                                << "$sin" << open_document
                                    << "$divide" << open_array
                                        << "$$delta_phi"
                                        << 2
                                    << close_array
                                << close_document
                            << close_document
                            << "sin_lambda" << open_document
                                << "$sin" << open_document
                                    << "$divide" << open_array
                                        << "$$delta_lambda"
                                        << 2
                                    << close_array
                                << close_document
                            << close_document
                        << close_document
                        << "in" << open_document
                            << "$let" << open_document
                                << "vars" << open_document
                                    << "temp_var" << open_document
                                        << "$add" << open_array
                                            << open_document
                                                << "$multiply" << open_array
                                                    << "$$sin_phi"
                                                    << "$$sin_phi"
                                                << close_array
                                            << close_document

                                            << open_document
                                                << "$multiply" << open_array
                                                    << open_document
                                                        << "$cos" << bsoncxx::types::b_double{ user_latitude*M_PI/180.0 }
                                                    << close_document

                                                    << open_document
                                                        << "$cos" << "$$phi_two"
                                                    << close_document

                                                    << open_document
                                                        << "$multiply" << open_array
                                                            << "$$sin_lambda"
                                                            << "$$sin_lambda"
                                                        << close_array
                                                    << close_document

                                                << close_array
                                            << close_document

                                        << close_array
                                    << close_document
                                << close_document
                                << "in" << open_document
                                    << "$multiply" << open_array
                                        << 7917.511731
                                        << open_document
                                            << "$atan2" << open_array
                                                << open_document
                                                    << "$sqrt" << "$$temp_var"
                                                << close_document

                                                << open_document
                                                    << "$sqrt" << open_document
                                                        << "$subtract" << open_array
                                                            << 1
                                                            << "$$temp_var"
                                                        << close_array
                                                    << close_document
                                                << close_document

                                            << close_array
                                        << close_document

                                    << close_array
                                << close_document
                            << close_document
                        << close_document
                    << close_document
                << close_document
            << close_document
        << finalize;
}