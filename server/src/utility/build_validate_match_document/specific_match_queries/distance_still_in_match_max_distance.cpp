//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"
#include "user_account_keys.h"

//check if distance is still in distance range of other
void distanceStillInMatchMaxDistance(bsoncxx::builder::stream::document& matchDoc,
                       const double& matchDistance) {
    matchDoc
                << user_account_keys::MAX_DISTANCE << bsoncxx::builder::stream::open_document
                    << "$gte" << bsoncxx::types::b_double{matchDistance}
                << bsoncxx::builder::stream::close_document;
}

