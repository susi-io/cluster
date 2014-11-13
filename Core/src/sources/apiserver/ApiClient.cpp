#include "apiserver/ApiClient.h"

void Susi::Api::ApiClient::publish( Susi::Events::EventPtr event, Susi::Events::Consumer finishCallback ) {
    LOG(DEBUG) << "publish called";
    PublishData data{*event,finishCallback};
    publishs[event->getID()] = data;
    sendPublish( *event );
    delete event.release();
}

long Susi::Api::ApiClient::subscribe( std::string topic, Susi::Events::Processor processor , std::string name) {
    LOG(DEBUG) << "sub processor called";
    sendRegisterProcessor( topic,name );
    subscribes.push_back(SubscribeData{topic,name,true});
    return Susi::Events::Manager::subscribe( topic,processor,name );
}

long Susi::Api::ApiClient::subscribe( std::string topic, Susi::Events::Consumer consumer , std::string name) {
    LOG(DEBUG) << "sub consumer called";
    sendRegisterConsumer( topic,name );
    subscribes.push_back(SubscribeData{topic,name,false});
    return Susi::Events::Manager::subscribe( topic,consumer,name );
}


void Susi::Api::ApiClient::onConsumerEvent( Susi::Events::Event & event ) {
    LOG(DEBUG) << "got consumer event: "<<event.toString();
    auto evt = createEvent( event.getTopic() );
    *evt = event;
    Susi::Events::Manager::publish( std::move( evt ) );
}

void Susi::Api::ApiClient::onProcessorEvent( Susi::Events::Event & event ) {
    LOG(DEBUG) << "got processor event: "<<event.toString();
    auto evt = createEvent( event.getTopic() );
    *evt = event;
    Susi::Events::Manager::publish( std::move( evt ),[this]( Susi::Events::SharedEventPtr event ) {
        sendAck( *event );
    } );
}

void Susi::Api::ApiClient::onAck( Susi::Events::Event & event ) {
    LOG(DEBUG) << "got ack: "<<event.toString();
    auto evt = std::make_shared<Susi::Events::Event>( event );
    auto publishData = publishs[evt->getID()];
    if( publishData.finishCallback ) {
        publishData.finishCallback( evt );
    }
    publishs.erase(evt->getID());
}

void Susi::Api::ApiClient::onReconnect() {
    LOG(DEBUG) << "we had a reconnect -> resubscribing everything";
    for(auto it : subscribes){
        if(it.processor){
            sendRegisterProcessor(it.topic,it.name);
        }else{
            sendRegisterConsumer(it.topic,it.name);
        }
    }
    for(auto kv : publishs){
        sendPublish(kv.second.event);
    }
}