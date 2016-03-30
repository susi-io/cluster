#include "susi/MongoDBComponent.h"

Susi::MongoDBComponent::MongoDBComponent(Susi::SusiClient & susi, const BSON::Value & config) : _susi{susi}, _config{config} {

    try{
        host     = _config["host"].getString();
        port     = _config["port"].getString();
        username = _config["username"].getString();
        password = _config["password"].getString();
        database = _config["database"].getString();
    }
    catch(const std::exception & e) {
        std::cout << e.what() << std::endl;
    }

    _susi.registerProcessor("mongodb::create", [this](Susi::EventPtr event){
        auto collection = event->payload["collection"].getString();
        auto data = event->payload["data"];
        create(collection, data);
        event->payload["success"] = true;
    });

    _susi.registerProcessor("mongodb::find", [this](Susi::EventPtr event){
        auto collection = event->payload["collection"].getString();
        auto query = event->payload["query"];
        event->payload["data"] = find(collection, query);
    });

    _susi.registerProcessor("mongodb::update", [this](Susi::EventPtr event){
        auto collection = event->payload["collection"].getString();
        auto findQuery = event->payload["findQuery"];
        auto updateQuery = event->payload["updateQuery"];
        update(collection, findQuery, updateQuery);
        event->payload["success"] = true;
    });

    _susi.registerProcessor("mongodb::remove", [this](Susi::EventPtr event){
        auto collection = event->payload["collection"].getString();
        auto removeQuery = event->payload["removeQuery"];
        remove(collection, removeQuery);
        event->payload["success"] = true;
    });
}


void Susi::MongoDBComponent::create(const std::string collectionName, const BSON::Value data) {
    bsoncxx::document::value document = bsoncxx::from_json(data.toJSON());
    auto collection = conn[database][collectionName];
    collection.insert_one(document.view());
}

std::vector<BSON::Value> Susi::MongoDBComponent::find(const std::string collectionName, const BSON::Value query) {
    bsoncxx::document::value document = bsoncxx::from_json(query.toJSON());
    auto collection = conn[database][collectionName];
    auto cursor = collection.find(document.view());

    std::vector<BSON::Value> data;
    for (auto&& doc : cursor) {
        data.push_back(BSON::Value::fromJSON(bsoncxx::to_json(doc)));
    }
    return data;
}

void Susi::MongoDBComponent::update(const std::string collectionName, const BSON::Value findQuery, const BSON::Value updateQuery) {
    bsoncxx::document::value update = bsoncxx::from_json(updateQuery.toJSON());
    bsoncxx::document::value find   = bsoncxx::from_json(findQuery.toJSON());

    auto collection = conn[database][collectionName];
    collection.update_many(find.view(), update.view());
}

void Susi::MongoDBComponent::remove(const std::string collectionName, const BSON::Value removeQuery) {
    bsoncxx::document::value removeMany = bsoncxx::from_json(removeQuery.toJSON());

    auto collection = conn[database][collectionName];
    collection.delete_many(removeMany.view());
}

BSON::Value Susi::MongoDBComponent::mapreduce(
	const std::string collectionName, const std::string map, const std::string reduce,
	const BSON::Value query, const BSON::Value sort, const int limit, const std::string finalize) {

    auto collection = conn[database];

	bsoncxx::document::value queryDoc = bsoncxx::from_json(query.toJSON());
	bsoncxx::document::value sortDoc = bsoncxx::from_json(sort.toJSON());

	bsoncxx::document::value command = bsoncxx::builder::stream::document{}
		<< "mapReduce" << collectionName
		<< "map" << map
		<< "reduce" << reduce
		<< "query" << queryDoc
		<< "sort" << sortDoc
		<< "limit" << limit
		<< "finalize" << finalize
		<< bsoncxx::builder::stream::finalize;
	auto result = collection.run_command(command.view());

    return bsoncxx::to_json(result);
}

Susi::MongoDBComponent::~MongoDBComponent() {
    join();
}

void Susi::MongoDBComponent::join() {
    _susi.join();
}
