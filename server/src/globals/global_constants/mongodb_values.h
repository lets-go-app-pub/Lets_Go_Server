//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include <mongocxx/uri.hpp>

#ifdef _RELEASE
#include "general_values.h"
#endif

namespace mongodb_values {

#ifdef _RELEASE
    //Production URI.
    inline const mongocxx::uri URI = mongocxx::uri(
            general_values::MONGODB_URI_STRING
    );
#else
    //Testing URI with local database and full access.
//    inline const mongocxx::uri URI = mongocxx::uri(
//            "mongodb://localhost:27017,localhost:27018,localhost:27019/?replicaSet=testReplicaSet&maxPoolSize=1000&retryWrites=true&w=1"
//    );

    //Testing URI with local database and access restrictions.
    inline const mongocxx::uri URI = mongocxx::uri(
            "mongodb://clientInterfaceLookalike:pass@localhost:27017,localhost:27018,localhost:27019/?replicaSet=testReplicaSet&maxPoolSize=1000&retryWrites=true&w=1"
    );

    //Testing URI with remote database and access restrictions.
//    inline const mongocxx::uri URI = mongocxx::uri(
//            "mongodb://hostwinds.replset.member:61132,digitalocean.replset.member:61132,linode.replset.member:61132"
//            "/?replicaSet=letsGoRs"
//            "&tls=true"
//            "&maxPoolSize=1000"
//            "&retryWrites=true"
//            "&w=1"
//            "&tlsCertificateKeyFile=/home/jeremiah/Documents/Lets_Go_Docs/Ssl_Key/MongoDB/Users/clientInterface.pem"
//            "&tlsCAFile=/home/jeremiah/Documents/Lets_Go_Docs/Ssl_Key/MongoDB/Certificate_Authority/ca.pem"
//            "&authSource=$external&authMechanism=MONGODB-X509"
//    );

#endif

}