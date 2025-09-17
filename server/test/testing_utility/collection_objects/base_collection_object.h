//
// Created by jeremiah on 5/30/22.
//

#pragma once

//parent class
class BaseCollectionObject {
public:
    bsoncxx::oid current_object_oid{"000000000000000000000000"};

    //will build the document from the values stored inside this class and upsert it into
    //the collection
    //will print any exceptions and return false if fails
    //returns true if successful
    virtual bool setIntoCollection() = 0;

    //will extract a document from the collection and save it to this class instance
    //returns false if fails and prints the error
    //returns true if successful
    virtual bool getFromCollection(const bsoncxx::oid& findOID) = 0;

    bsoncxx::types::b_date DEFAULT_DATE = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
};

template<typename T>
void checkForEquality(
        const T& first,
        const T& second,
        const std::string& key,
        const std::string& object_class_name,
        bool& return_value
        ) {
    if (first != second) {
        return_value = false;
        std::cout << "ERROR: " << object_class_name << ' ' << key << " Variables Do Not Match\n"
        << first << '\n'
        << second << '\n';
    }
}