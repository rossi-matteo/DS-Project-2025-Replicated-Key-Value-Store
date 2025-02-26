//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "DatastoreServer.h"

DatastoreServer::DatastoreServer() {} //TODO?

DatastoreServer::~DatastoreServer() {} //TODO?

void DatastoreServer::initalize(){
    serverId = par("serverId");
    totalServers = par("totalServers");

    for(int i = 0; i < totalServers; i++)
        vectorClock[i] = 0;

    updateIdCounter = 0;

    EV << "Server[" << serverId << "] starting" << endl;
}


void DatastoreServer::handleMessage(cMessage *msg){

    if(msg -> isSelfMessage()){
        if(msg == heartbeatTimer) {
            //sendHeartbeats();
            scheduleAt(simTime() + par("messageDelay").value() * 10, heartbeatTimer);
        }
        return;
    }

    NetworkMessages *inboundMsg = check_and_cast<NetworkMessages *>(msg);

    if(dynamic_cast<ReadRequestMsg *>(inboundMsg)){
        handleRead(check_and_cast<ReadRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<WriteRequestMsg *>(inboundMsg)){
        handleWrite(check_and_cast<WriteRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<UpdateRequestMsg *>(inboundMsg)){
        handleRead(check_and_cast<UpdateRequestMsg *>(inboundMsg));
    }
    /* TODO All the other messages here
    else if(dynamic_cast<XXXRequestMsg *>(inboundMsg)){
        handleRead(check_and_cast<XXRequestMsg *>(inboundMsg));
    }
     */
}

void DatastoreServer::finish(){
    EV << "Server[" << serverId << "]" << endl;

    EV << "Final Datastore Status <Key, Value>" << endl;
    for(auto tuple : store){
        EV << tuple.first << ": " << tuple.second << endl;
    }

    EV << "Vector Clock: " << endl << "[";
    for(int index = 0; i < vectorClock.size()-1; i++){
        EV << vectorClock[i] << ", ";
    }
    EV << vectorClock[i] << " ]" << endl;
}

void DatastoreServer::handleRead(ReadRequestMsg *msg){

}

void DatastoreServer::handleWrite(WriteRequestMsg *msg){

}

void DatastoreServer::handleUpdate(UpdateRequestMsg *msg){

}

void DatastoreServer::applyUpdate(){

}

void DatastoreServer::sendUpdate(std::string key, int value){

}

void DatastoreServer::checkPendingUpdates(){

}

bool DatastoreServer::checkCausalDependencies(){

}





























