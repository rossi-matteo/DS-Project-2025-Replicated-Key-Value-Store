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

DatastoreServer::DatastoreServer() {
    heartbeatTimer =nullptr;
}

DatastoreServer::~DatastoreServer() {
    cancelAndDelete(heartbeatTimer);
    cancelAndDelete(retransmissionTimer);
}

void DatastoreServer::initialize(){
    serverId = par("serverId");
    totalServers = par("totalServers");

    for(int i = 0; i < totalServers; i++)
        vectorClock[i] = 0;

    updateIdCounter = 0;

    EV << "Server[" << serverId << "] starting" << endl;

    heartbeatTimer = new cMessage("heartbeatTimer");
    scheduleAt(simTime(), heartbeatTimer);

    retransmissionTimer = new cMessage("retransmissionTimer");
    scheduleAt(simTime() + par("retransmissionInterval").doubleValue(), retransmissionTimer);
}


void DatastoreServer::handleMessage(cMessage *msg){

    if(msg -> isSelfMessage()){
        if(msg == heartbeatTimer) {
            sendHeartbeats();
            scheduleAt(simTime() + par("heartbeatInterval").doubleValue(), heartbeatTimer);
        } else if(msg == retransmissionTimer) {
            retransmitUnackedUpdates();
            scheduleAt(simTime() + par("retransmissionInterval").doubleValue(), retransmissionTimer);
        }
        return;
    }

    NetworkMsg *inboundMsg = check_and_cast<NetworkMsg *>(msg);

    if(dynamic_cast<ReadRequestMsg *>(inboundMsg)){
        handleRead(check_and_cast<ReadRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<WriteRequestMsg *>(inboundMsg)){
        handleWrite(check_and_cast<WriteRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<UpdateMsg *>(inboundMsg)){
        handleUpdate(check_and_cast<UpdateMsg *>(inboundMsg));
    }
    else if(dynamic_cast<UpdateAckMsg *>(inboundMsg)){
         ; //TODO
    }
    else if(dynamic_cast<HeartbeatMsg *>(inboundMsg)){
        handleHeartbeat(check_and_cast<HeartbeatMsg *>(inboundMsg));
    }

    delete inboundMsg;
}

void DatastoreServer::finish(){
    EV << "Server[" << serverId << "]" << endl;

    EV << "Final Datastore Status <Key, Value>" << endl;
    for(auto tuple : store){
        EV << tuple.first << ": " << tuple.second << endl;
    }

    int i;
    EV << "Vector Clock: " << endl << "[";
    for(i = 0; i < vectorClock.size()-1; i++){
        EV << vectorClock[i] << ", ";
    }
    EV << vectorClock[i] << " ]" << endl;
}

void DatastoreServer::handleRead(ReadRequestMsg *msg){
    std::string key = msg->getKey();
    int value = 0; //TODO: Handle read of key not present

    if(store.find(key) != store.end()){
        value = store[key];
    }

    ReadResponseMsg *response = new ReadResponseMsg();
    response -> setSourceId(serverId);
    response -> setKey(key.c_str());
    response -> setValue(value);

    // Send response back to client
    send(response, "clientChannels$o", msg->getSourceId() % par("numClientsPerServer").intValue());

    EV << "Server " << serverId << " performed Read <" << key
       << ", " << value << ">" << endl;
}

void DatastoreServer::handleWrite(WriteRequestMsg *msg){
    std::string key = msg->getKey();
    int value = msg->getValue();
    int sourceId = msg->getSourceId();

    // Apply write locally and update this server's vector clock
    store[key] = value;
    vectorClock[serverId]++;

    EV << "Server " << serverId << " applied update for <key,value>: " << key << ", " << value << " from " << sourceId << endl;
    
    WriteResponseMsg *writeResponse = new WriteResponseMsg();
    writeResponse -> setSourceId(serverId);
    writeResponse -> setKey(key.c_str());
    send(writeResponse, "clientChannels$o", msg->getSourceId() % par("numClientsPerServer").intValue());

    // Propagate to other servers
    sendUpdate(key, value);

    EV << "Server " << serverId << " performed Write <" << key
       << ", " << value << ">" << endl;
}

bool DatastoreServer::isSatisfyingCausalDependencies(int sourceId, std::map<int, int> senderVectorClock){
    // For every node other than the sender, check if the sender's clock is ahead of ours

    EV << "==RECEIVER VC==" << endl;
    for(auto entry : vectorClock)
        EV << entry.first << "," << entry.second << endl;
    EV << "==SENDER VC==" << endl;
    for(auto entry : senderVectorClock)
        EV << entry.first << "," << entry.second << endl;
    EV << "==========" << endl;


    for(auto entry : senderVectorClock) { 
        if(entry.first == sourceId) 
            continue;
        
        if (vectorClock[entry.first] < entry.second) // Violating causal dependency since sender has seen more recent updates
            return false;
    }

    // Check that the sender's clock is exactly one ahead of the receiver's
    return senderVectorClock.at(sourceId) == vectorClock[sourceId] + 1;
}

void DatastoreServer::handleUpdate(UpdateMsg *msg){
    std::string key = msg->getKey();
    int value = msg->getValue();
    int sourceId = msg->getSourceId();
    int updateId = msg->getUpdateId();

    std::map<int, int> senderVectorClock(msg->getVectorClock());

    // Check if the same update has already been received
    std::pair<int, int> updateInformation = {sourceId, updateId};
    if (receivedUpdates.contains(updateInformation)) {
        EV << "Server " << serverId << " ignoring duplicate update " << updateId << " from server " << sourceId << endl;
        
        // Still send back ack in case original got lost
        UpdateAckMsg *ack = new UpdateAckMsg();
        ack->setSourceId(serverId);
        ack->setUpdateId(updateId);
        int gateIndex = (sourceId > serverId) ? sourceId - 1 : sourceId;
        send(ack, "serverChannels$o", gateIndex);

        return;
    }

    // Add the update information to the receivedUpdates set
    receivedUpdates.insert(updateInformation);

    // Check if the update is causally dependent
    if(isSatisfyingCausalDependencies(sourceId, senderVectorClock)){
        // Apply update locally
        EV << "Server " << serverId << " applying update from server " << sourceId 
        << " for key " << key << endl;
            
        //Apply Update
        store[key] = value;
        vectorClock[sourceId] = senderVectorClock[sourceId];

        checkPendingUpdates();
    }else{
        // Store the update in the pending updates list
        EV << "Server " << serverId << " queuing update from server " << sourceId 
        << " for key: " << key << " (dependencies not satisfied)" << endl;

        PendingUpdate newPendingUpdate;
        newPendingUpdate.key = key;
        newPendingUpdate.value = value;
        newPendingUpdate.sourceId = sourceId;
        newPendingUpdate.senderVectorClock = std::map(senderVectorClock);
        newPendingUpdate.updateId = msg->getUpdateId();
        pendingUpdates.push_back(newPendingUpdate);
    }
    
    // Send ack back to the sender
    UpdateAckMsg *updateResponse = new UpdateAckMsg();
    updateResponse->setSourceId(serverId);
    updateResponse->setUpdateId(updateId);
    //updateAck.setTargetServerId(...);
    
    int gateIndex = (sourceId > serverId) ? sourceId-1 : sourceId;

    send(updateResponse, "serverChannels$o", gateIndex);
}

void DatastoreServer::handleUpdateAck(UpdateAckMsg *msg) {
    int fromServer = msg->getSourceId();
    int updateId = msg->getUpdateId();

    if (unackedUpdates[fromServer].contains(updateId)) {
        delete unackedUpdates[fromServer][updateId].msg; // Clean up the old message
        unackedUpdates[fromServer].erase(updateId); // Remove the message from the unacked ones

        EV << "Server " << serverId << " received ack for update " << updateId << " from server " << fromServer << endl;
    }
}

void DatastoreServer::sendUpdate(std::string key, int value){
    //Create an UpdateMsg for all the other servers
    for(int i = 0; i < gateSize("serverChannels$o"); i++){

        if(!onlineDatastores.contains(i)){
            EV << "Server " << serverId << " skipping update propagation to disconnected server " << i << endl;
            continue;
        }

        UpdateMsg *updateMsg = new UpdateMsg();
        updateMsg->setSourceId(serverId);
        updateMsg->setKey(key.c_str());
        updateMsg->setValue(value);
        updateMsg->setUpdateId(updateIdCounter++);
        updateMsg->setVectorClock(vectorClock);

        /*
        TODO: Apply random failure
        if(uniform(0, 1) < par("failureProbability").doubleValue()){
            EV << "Server " << serverId << " simulating message loss to server " << i << endl;
            delete updateMsg;
            continue;
        }
        */ 

        send(updateMsg->dup(), "serverChannels$o", i);

        // TODO are we sure that i is the server's id?
        unackedUpdates[i][updateMsg->getUpdateId()] = {updateMsg, simTime()}; // Add the newly created update to the unacked ones

        EV << "Server " << serverId << " sent update for <" << key << ", " << value << "> to server " << i << endl;
    }
}

void DatastoreServer::checkPendingUpdates(){
    bool applied = false;

    do{
        applied = false;

        for(auto it = pendingUpdates.begin(); it != pendingUpdates.end(); ){
            if(isSatisfyingCausalDependencies(it->sourceId, it->senderVectorClock)){
                // Apply update locally
                EV << "Server " << serverId << " applying queued update from server " << it->sourceId 
                << " for key " << it->key << endl;
                
                //Apply Update
                store[it->key] = it->value;
                vectorClock[it->sourceId] = it->senderVectorClock[it->sourceId];
                appliedUpdates.insert({it->sourceId, it->updateId});

                // Remove the update from the pending list
                it = pendingUpdates.erase(it);

                applied = true;
            } else {
                it++;
            }   
        }
    } while(applied);

}


void DatastoreServer::applyUpdate(int sourceId, std::string key, int value){
}


void DatastoreServer::handleHeartbeat(HeartbeatMsg *msg) {
    // Update the online datastores list

    // TODO: Verify if the check on staleness of the heartbeat is necessary [ (messageDelay = default(uniform(0.1s,0.5s))) * 10]
    if(msg->getTimestamp() - simTime() > par("maxAllowedHeartbeatDelay").doubleValue()){
        onlineDatastores.erase(msg->getSourceId());
        return;
    }

    onlineDatastores.insert(msg->getSourceId());

}

void DatastoreServer::sendHeartbeats() {
    //Send heartbeat to all other servers
    for(int gateIndex = 0; gateIndex < gateSize("serverChannels$o"); gateIndex++){

        HeartbeatMsg *heartbeatMsg = new HeartbeatMsg();
        heartbeatMsg->setSourceId(serverId);
        heartbeatMsg->setTimestamp(simTime().dbl());

        send(heartbeatMsg, "serverChannels$o", gateIndex);
    }
}

void DatastoreServer::retransmitUnackedUpdates(){
    for(auto &serverPair : unackedUpdates){
        int targetServer = serverPair.first;

        for(auto &updatePair : serverPair.second){
            int updateId = updatePair.first;
            SentUpdate &update = updatePair.second;

            if(simTime() - update.timestamp >= par("retransmissionInterval").doubleValue()){
                send(update.msg->dup(), "serverChannels$o", targetServer);
                update.timestamp = simTime();

                EV << "Server " << serverId << " retransmitting update " << updateId << " to server " << targetServer << endl;
            }
        }
    }
}
























