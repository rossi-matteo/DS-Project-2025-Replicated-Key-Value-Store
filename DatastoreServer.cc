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
    heartbeatTimer = nullptr;
    networkPartitionEventTimer = nullptr;
}

DatastoreServer::~DatastoreServer() {
    cancelAndDelete(heartbeatTimer);
    cancelAndDelete(networkPartitionEventTimer);
}

void DatastoreServer::initialize(){
    serverId = par("serverId");
    totalServers = par("totalServers");
    omissionFailureProbability = par("omissionFailureProbability").doubleValue();
    createNetworkPartitionProbability = par("createNetworkPartitionProbability").doubleValue();
    terminateNetworkPartitionProbability = par("terminateNetworkPartitionProbability").doubleValue();
    networkPartitionLinkProbability = par("networkPartitionLinkProbability").doubleValue();
    activeNetworkPartition = false;

    isChannelAffectedByNetworkPartition = (bool*) malloc(sizeof(bool) * (totalServers - 1));

    for(int i = 0; i < totalServers; i++){
        vectorClock[i] = 0;
    }

    for(int i = 0; i < totalServers-1; i++){
        isChannelAffectedByNetworkPartition[i] = false;
    }

    EV_INFO << "[SERVER-" << serverId << "] Starting" << endl;

    heartbeatTimer = new cMessage("heartbeatTimer");
    scheduleAt(simTime(), heartbeatTimer);

    networkPartitionEventTimer = new cMessage("networkPartitionEventTimer");
    scheduleAt(simTime(), networkPartitionEventTimer);

    // Initialize lastKnownVectorClocks map
    for(int i = 0; i < totalServers; i++){
        std::map<int, int> map;
        
        // For all servers except this one, initialize the vector clock to 0
        if(i == serverId){
            map = vectorClock; //In this case, the map will be the same as the vector clock of this server
        } else {
            // For the other datastores, initialize the vector clock to 0 instead
            for(int j = 0; j < totalServers; j++)
                map[j] = 0;
        }

        lastKnownVectorClocks[i] = map; // Pointer to vector clock
    }
    
    // Initialization of vector clock used for deleting updates applied by everyone
    for(int i = 0; i < totalServers; i++){
        prevMinVectorClock[i] = 0;
    }

    for(int i = 0; i < totalServers; i++){
        onlineDatastores[i] = SIMTIME_ZERO;
    }
}

void DatastoreServer::handleMessage(cMessage *msg){

    if(msg -> isSelfMessage()){
        if(msg == heartbeatTimer) {
            sendHeartbeats();
            scheduleAt(simTime() + par("heartbeatInterval").doubleValue(), heartbeatTimer);
        } else if (msg == networkPartitionEventTimer){
            if(activeNetworkPartition && terminateNetworkPartitionProbability > uniform(0,1)){

                activeNetworkPartition = false;
                for( int i = 0; i < totalServers; i++){
                    isChannelAffectedByNetworkPartition[i] = false;
                }

                EV_INFO << "[SERVER-" << serverId << "] Network Partition Terminated ";

            } else if(!activeNetworkPartition && createNetworkPartitionProbability > uniform(0,1)) {

                // Ensure at least one link is affected
                int randomIndex = intuniform(0, totalServers - 1);
                isChannelAffectedByNetworkPartition[randomIndex] = true;

                for (int i = 0; i < totalServers - 1; i++) {
                    if ( (uniform(0, 1) < par("networkPartitionLinkProbability").doubleValue()) && (i != randomIndex) )
                        isChannelAffectedByNetworkPartition[i] = true;
                    
                }

                EV_INFO << "[SERVER-" << serverId << "] Network Partition Created | Affected Links: ";
                for (int i = 0; i < totalServers - 1; i++) {
                    if (isChannelAffectedByNetworkPartition[i])
                        EV_INFO << fromGateToServer(i) << " ";
                }
                EV_INFO << endl;
                
                activeNetworkPartition = true;
            }

            scheduleAt(simTime() + par("networkPartitionEventInterval").doubleValue(), networkPartitionEventTimer);
        }
        return;
    }

    NetworkMsg *inboundMsg = check_and_cast<NetworkMsg *>(msg);

    cChannel *chan = msg->getArrivalGate()->getChannel();

    // Check if the inbound message is coming from a link affected by the Network Partition, if present
    if(activeNetworkPartition && parseMessageToRetrieveSenderModuleClass(msg) == "SERVER"){
        if( isChannelAffectedByNetworkPartition[ fromServerToGate(inboundMsg->getSourceId()) ] ){
        EV_INFO << "[SERVER-" << serverId << "] Incoming Packet Lost | Cause: Network Partition | From: [SERVER-" << inboundMsg->getSourceId() << "] | Type: " << inboundMsg->getClassName() << endl;
        delete inboundMsg;
        return;
        }
    }

    //Apply Omission failures to incoming packet with a certain probability
    if( omissionFailureProbability > uniform(0,1)) {
        EV_INFO << "[SERVER-" << serverId << "] Incoming Packet Lost | Cause: Omission | From: [" << parseMessageToRetrieveSenderModuleClass(msg) << "-" << inboundMsg->getSourceId() << "] | Type: " << inboundMsg->getClassName() << endl;
        delete inboundMsg;
        return;
    }

    if(dynamic_cast<ReadRequestMsg *>(inboundMsg)){
        handleRead(check_and_cast<ReadRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<WriteRequestMsg *>(inboundMsg)){
        handleWrite(check_and_cast<WriteRequestMsg *>(inboundMsg));
    }
    else if(dynamic_cast<WritePropagationMsg *>(inboundMsg)){
        handleUpdate(check_and_cast<WritePropagationMsg *>(inboundMsg));
    }
    else if(dynamic_cast<HeartbeatMsg *>(inboundMsg)){
        handleHeartbeat(check_and_cast<HeartbeatMsg *>(inboundMsg));
    }
    else if(dynamic_cast<MissingWritesRequestMsg *>(inboundMsg)){
        handleMissingWrites(check_and_cast<MissingWritesRequestMsg *>(inboundMsg));
    }

    delete inboundMsg;
}

void DatastoreServer::finish(){
    EV_INFO << "[SERVER-" << serverId << "] Terminating" << endl;

    EV_INFO << "[SERVER-" << serverId << "] Final Datastore Status <Key, Value>" << " | ";
    for(auto tuple : store){
        EV_INFO << tuple.first << "," << tuple.second << "; ";
    }

    int i;
    EV_INFO << "[SERVER-" << serverId << "] Vector Clock | ";
    for(i = 0; i < vectorClock.size(); i++){
        EV_INFO << i << "," << vectorClock[i] << "; ";
    }
    EV_INFO << endl;
}

void DatastoreServer::handleRead(ReadRequestMsg *msg){
    std::string key = msg->getKey();

    /* Default value used to handle reads of non-existing keys */
    int value = 0; 

    if(store.find(key) != store.end()){
        value = store[key];
    }

    ReadResponseMsg *response = new ReadResponseMsg();
    response -> setSourceId(serverId);
    response -> setKey(key.c_str());
    response -> setValue(value);

    // Send response back to client
    int clientId = msg->getSourceId() % par("numClientsPerServer").intValue();
    send(response, "clientChannels$o", clientId);

    EV_INFO << "[SERVER-" << serverId << "] Read Performed | From: [CLIENT-" << msg->getSourceId() << "] | <"
       << key << ", " << value << ">" << endl;
}

void DatastoreServer::handleWrite(WriteRequestMsg *msg){
    std::string key = msg->getKey();
    int value = msg->getValue();
    int sourceId = msg->getSourceId();

    // Apply write locally and update this server's vector clock
    store[key] = value;
    vectorClock[serverId]++;

    // Update our last known vector clock
    lastKnownVectorClocks[serverId] = vectorClock;

    EV_INFO << "[SERVER-" << serverId << "] Apply Write | From: [CLIENT-" << msg->getSourceId() << "] | <" << key << ", " << value << ">" << endl;
    
    // Store the received Write Propagation in our internal cache
    storeWrite(key, value, serverId, vectorClock);

    // Notify the client that the write has been performed
    WriteResponseMsg *writeResponse = new WriteResponseMsg();
    writeResponse -> setSourceId(serverId);
    writeResponse -> setKey(key.c_str());

    // Send the response to the client
    int clientId = msg->getSourceId() % par("numClientsPerServer").intValue();
    send(writeResponse, "clientChannels$o", clientId);

    // Propagate the received write to other servers
    sendUpdate(key, value);
}

int DatastoreServer::isSatisfyingCausalDependencies(int sourceId, std::map<int, int> senderVectorClock){
    // For every server other than the sender, check if the sender's clock is ahead of ours

    EV_INFO << "[SERVER-" << sourceId << "] Sender VC | SourceID: " << sourceId << " | [";
    for(auto entry : senderVectorClock)
        EV_INFO << entry.first << "," << entry.second << ";";
    EV_INFO << "] | [SERVER-" << serverId << "] Receiver VC" << " | [";
    for(auto entry : vectorClock)
        EV_INFO << entry.first << "," << entry.second << ";";
    EV_INFO << "]" << endl;

    // Causal Dependency Check
    for(const auto& [id, value] : senderVectorClock) { 
        if(id == sourceId) 
            continue;
        
        if (vectorClock[id] < value){ // Violating causal dependency since sender has seen more recent updates
            return 0; //Dependencies Not Satisfied
        }
    }

    // Message Older Than Current State Check
    // Check if the sender's vector clock is older than the receiver's
    bool isOlder = true;
    for(const auto& [id, value] : senderVectorClock){
        if(vectorClock[id] < value){
            isOlder = false;
            break;
        }
    }
    if(isOlder){
        return -1;
    }

    // Check that the sender's clock is exactly one ahead of the receiver's
    if(senderVectorClock.at(sourceId) == vectorClock[sourceId] + 1)
        return 1; //Write is Causal Dependent
    else
        return 0;
}

void DatastoreServer::handleUpdate(WritePropagationMsg *msg){
    std::string key = msg->getKey();
    int value = msg->getValue();
    int sourceId = msg->getSourceId();
    std::map<int, int> senderVectorClock = msg->getVectorClock();

    // If the received sender's vector clock is the latest by the sender, update the lastKnownVectorClocks map
    if(senderVectorClock[sourceId] > lastKnownVectorClocks[sourceId][sourceId])
        lastKnownVectorClocks[sourceId] = std::map<int, int>(senderVectorClock);

    // Store the received Write Propagation in our internal cache
    Update propagatedWrite = storeWrite(key, value, sourceId, senderVectorClock);

    switch(isSatisfyingCausalDependencies(sourceId, senderVectorClock)){
    case 1: //Write is Causal Dependent -> Apply Update
    {
        
        // Apply Update
        store[key] = value;
        vectorClock[sourceId] = senderVectorClock[sourceId];
        EV_INFO << "[SERVER-" << serverId << "] Apply Write Propagation | From: [SERVER-" << msg->getSourceId() << "] | <" << key << ", " << value << ">" << endl;

        checkPendingUpdates();

        // If the update was previously stored in the pending updates list, remove it
        auto it = std::find_if(pendingUpdates.begin(), pendingUpdates.end(),
            [key, value, senderVectorClock](const Update& u) { return (u.key == key) && (u.value == value) && (u.senderVectorClock == senderVectorClock); });
        if (it != pendingUpdates.end()) {
            pendingUpdates.erase(it);
            EV_INFO << "[SERVER-" << serverId << "] Remove Queued Write | From: [SERVER-" << it->sourceId << "] | <" << it->key << ", " << it->value << ">" << endl;
        }

        // TODO: For performance purposes, perform this operation after a batch of data has been processed instead of every time
        deleteUpdatesAppliedByEveryone();
        break;
    }
    case 0:
    {    //Dependencies aren't satisfied, some writes are missing -> Store Write in Pending, Ask for Missing Writes
         // Store the update in the pending updates list
         EV_INFO << "[SERVER-" << serverId << "] Queue Write | From: [SERVER-" << msg->getSourceId() << "] | <" << key << ", " << value << "> | (Dependencies Not Satisfied)" << endl;
    
         pendingUpdates.push_back(propagatedWrite);
 
         //I'm out of sync with the updates. I have to ask others to eventually retransmit missing infos
         // (even if these messages are only slow and still traveling on network?) - In general, yes
         // But I would like to avoid asking many many times for the same information
 
         std::map<int, std::set<int>> missingWrites;
         
         for(int i = 0; i < totalServers; i++){
             if(i == serverId)
                 continue;
 
             // Populate the map with the missing updates
             if(vectorClock[i] < senderVectorClock[i]){
                 int lowestUpdateId = vectorClock[i];
                 for(int j = lowestUpdateId+1; j <= senderVectorClock[i]; j++){
                     // Find the Update with the given updateId (i.e. the int at vectorClock[sourceId]) and sent by the the server 
                     // with its ID equal to sourceId. This guarantees that the (possibly) found update is the one we were looking for.
                     auto it = std::find_if(pendingUpdates.begin(), pendingUpdates.end(),
                         [j, i](const Update& u) { return (u.senderVectorClock.at(u.sourceId) == j) && (u.sourceId == i); });
 
                     // If the update has not been found in the pending ones, add its ID to the missing writes to be requested
                     if (it == pendingUpdates.end())
                         missingWrites[i].insert(j);
                 }
                 
             }
 
         }
         
         // Create a message containing all the writes that this server is missing and send it to all other online datastores
         MissingWritesRequestMsg *mwrMsg = new MissingWritesRequestMsg();
         mwrMsg->setSourceId(serverId);
         mwrMsg->setMissingWrites(std::map<int, std::set<int>>(missingWrites));

         std::string strategy = par("missingWritesRequestStrategy").stringValue();

         if (strategy.compare(0, 8, "flooding") == 0) {
            // Send to all other servers
            for(int i = 0; i < totalServers; i++){
                if(i == serverId || !getOnlineDatastores().contains(i))
                    continue;

                sendServerCheckPartition(mwrMsg->dup(), "serverChannels$o", fromServerToGate(i));
            }
            EV_INFO << "[SERVER-" << serverId << "] Ask Missing Writes | (Flooding) | #Contacted: " << totalServers - 1 << endl;

         } else if (strategy.compare(0, 6, "linear") == 0) {
            double maxNodesToContact = std::min(static_cast<double>(totalServers-1), par("maxNodesToContact").doubleValue());

            // Create a set of candidate servers (listed by ID) to contact
            // Filter the set by excluding the server itself and the ones that are offline
            // If inside a partition, exclude the ones not having an active channel

            std::set<int> candidateServers;
            for(int i = 0; i < totalServers; i++){
                if(i == serverId || !getOnlineDatastores().contains(i) || (activeNetworkPartition && isChannelAffectedByNetworkPartition[i]))
                    continue;

                candidateServers.insert(i);
            }

            // Uniform Random Bit Generator
            std::random_device rd;
            std::mt19937 g(rd());

            // Randomly shuffle the candidate servers
            std::vector<int> shuffledServers(candidateServers.begin(), candidateServers.end());
            std::shuffle(shuffledServers.begin(), shuffledServers.end(), g);

            int contacted = 0;

            // Send to all the remaining servers in the set
            for( auto it = shuffledServers.begin(); it != shuffledServers.end() && contacted < maxNodesToContact; ++it){
                int i = *it;

                contacted++;
                sendServerCheckPartition(mwrMsg->dup(), "serverChannels$o", fromServerToGate(i));
            }
            EV_INFO << "[SERVER-" << serverId << "] Ask Missing Writes | (Linear) | #Contacted: " << contacted << endl;

         } else {
            EV_INFO << "Fatal Error! | Unknown MissingWritesRequestStrategy" << endl;
            exit(-1);
         }

         delete mwrMsg;
 
         break;
    }
    case -1: //Write is older than the current state of the Data Store -> Discard
    {   
        EV_INFO << "[SERVER-" << serverId << "] Write Propagation Ignored | (No New Information)" << endl;
        break;
    }
    default:
    {
        EV_INFO << "Fatal Error!" << endl;
        exit(-1);
    }
    }
  
}

void DatastoreServer::sendUpdate(std::string key, int value){
    //Create an UpdateMsg for all the other servers to inform them of the received write
    for(int i = 0; i < gateSize("serverChannels$o"); i++){

        int serverIndex = fromGateToServer(i);

        if(!getOnlineDatastores().contains(serverIndex)){
            EV_INFO << "[SERVER-" << serverId << "] Skip Propagate Write | To: [SERVER-" << serverIndex << "] | (Offline)" <<endl;
            continue;
        }

        WritePropagationMsg *writePropagationMsg = new WritePropagationMsg();
        writePropagationMsg->setSourceId(serverId);
        writePropagationMsg->setKey(key.c_str());
        writePropagationMsg->setValue(value);
        writePropagationMsg->setVectorClock(std::map<int,int>(vectorClock));

        sendServerCheckPartition(writePropagationMsg, "serverChannels$o", i);

        EV_INFO << "[SERVER-" << serverId << "] Propagate Write | To: [SERVER-" << serverIndex << "] | <" << key << ", " << value << ">" << endl;
    }
}

void DatastoreServer::checkPendingUpdates(){
    // Check if the server is already storing updates that after the current one, do not violate the causal dependecies anymore and now can be applied
    // to the local store. If so, apply them and remove them from the pending updates list.

    bool applied = false;

    EV_INFO << "[SERVER-" << serverId << "] Check Pending Writes" << endl;
    do{
        applied = false;

        for(auto it = pendingUpdates.begin(); it != pendingUpdates.end(); ){

            int causalDepStatusResult = isSatisfyingCausalDependencies(it->sourceId, it->senderVectorClock);

            if(causalDepStatusResult == 1){
                // Apply update locally
                EV << "[SERVER-" << serverId << "] Apply Queued Write | From: [SERVER-" << it->sourceId << "] | <" << it->key << ", " << it->value << ">" << endl;
                
                //Apply Update
                store[it->key] = it->value;
                vectorClock[it->sourceId] = it->senderVectorClock[it->sourceId];

                // Remove the update from the pending list
                it = pendingUpdates.erase(it);

                applied = true;
            } else if (causalDepStatusResult == -1) {
                EV << "[SERVER-" << serverId << "] Remove Queued Write | From: [SERVER-" << it->sourceId << "] | <" << it->key << ", " << it->value << "> | (Old Information)" << endl;

                it = pendingUpdates.erase(it);
            } else {
                it++;
            }   
        }
    } while(applied);

}


void DatastoreServer::handleMissingWrites(MissingWritesRequestMsg *msg){
    // Collect the missing write and send them to the requesting server

    int sourceId = msg->getSourceId();
    std::map<int, std::set<int>> missingWrites = msg->getMissingWrites();

    int sourceChannel = fromServerToGate(sourceId);

    // Iterate over the sets of missing writes
    EV_INFO << "[SERVER-" << serverId << "] Checking Have Missing Writes | From: [SERVER-" << msg->getSourceId() << "]" << endl;
    for (const auto& [currServerId, updateIdSet] : missingWrites) {
        // Skip if it's the clock of the source (since the source should have its latest updates)
        if (currServerId == sourceId)
            continue;

        auto knownUpdates = receivedUpdates[currServerId];

        for(auto updateId : updateIdSet) {
            auto update = knownUpdates.find(updateId);

            if(update != knownUpdates.end()){
                WritePropagationMsg *updateMsg = new WritePropagationMsg();
                updateMsg -> setSourceId(update->second.sourceId);
                updateMsg -> setKey(update->second.key.c_str());
                updateMsg -> setValue(update->second.value);
                updateMsg -> setVectorClock(update->second.senderVectorClock);

                sendServerCheckPartition(updateMsg, "serverChannels$o", sourceChannel);
                EV_INFO << "[SERVER-" << serverId << "] Send Missing Write | To: [SERVER-" << sourceId
                   << "] | <" << update->second.key << ", " << update->second.value << ">" << " | Source, ID: " << update->second.sourceId << ", " << updateId << endl;
            }
        }
    }
}

void DatastoreServer::handleHeartbeat(HeartbeatMsg *msg) {
    if(msg->getTimestamp() > onlineDatastores[msg->getSourceId()]) {
        onlineDatastores[msg->getSourceId()] = msg->getTimestamp();
    }
    EV_INFO << "[SERVER-" << serverId << "] Received Heartbeat | From: [SERVER-" << msg->getSourceId() << "]" << endl;
}

std::set<int> DatastoreServer::getOnlineDatastores() {
    std::set<int> online;

    for (const auto& [id, timestamp]: onlineDatastores) {
        if (simTime() - timestamp <= par("maxAllowedHeartbeatDelay").doubleValue())
            online.insert(id);
    }
    
    return online;
}

void DatastoreServer::sendHeartbeats() {
    //Send heartbeat to all other servers
    for(int gateIndex = 0; gateIndex < gateSize("serverChannels$o"); gateIndex++){

        HeartbeatMsg *heartbeatMsg = new HeartbeatMsg();
        heartbeatMsg->setSourceId(serverId);
        heartbeatMsg->setTimestamp(simTime().dbl());

        sendServerCheckPartition(heartbeatMsg, "serverChannels$o", gateIndex);
    }
}

void DatastoreServer::deleteUpdatesAppliedByEveryone() {
    std::map<int, int> minimumVectorClock;

    // Initialize minimumVectorClock with maximum possible values
    for (int i = 0; i < totalServers; i++) {
        minimumVectorClock[i] = INT_MAX;
    }

    // Compute the minimum value for each field in the vector clock
    for(int vectorClockPosition = 0; vectorClockPosition < totalServers; vectorClockPosition++)
        for (int server = 0; server < totalServers; server++)
            minimumVectorClock[vectorClockPosition] = std::min(minimumVectorClock[vectorClockPosition], lastKnownVectorClocks[server][vectorClockPosition]);


    for(const auto& [serverIdx, value] : minimumVectorClock) {
        for(int i = prevMinVectorClock[serverIdx] + 1; i <= value; i++){
            receivedUpdates[serverIdx].erase(i);
            EV_INFO << "[SERVER-" << serverId << "] Delete Write | Source, ID: " << serverIdx << ", " << i << " | (It has been applied by everyone)" << endl;
        }
    }

    prevMinVectorClock = minimumVectorClock;
}

DatastoreServer::Update DatastoreServer::storeWrite(std::string key, int value, int serverId, std::map<int,int> vectorClock){

    Update update;
    update.key = key;
    update.value = value;
    update.sourceId = serverId;
    update.senderVectorClock = std::map(vectorClock);

    // Save the received update in the receivedUpdates map
    int updateId = vectorClock[serverId];
    receivedUpdates[serverId][updateId] = update;

    return update;
}


inline int DatastoreServer::fromServerToGate(int id){
    return id > serverId ? id - 1 : id;
}

inline int DatastoreServer::fromGateToServer(int index){
    return index >= serverId ? index + 1 : index;
}

std::string DatastoreServer::parseMessageToRetrieveSenderModuleClass(cMessage *msg){
    auto className = msg->getSenderModule()->getClassName();

    if (std::string(className).find("DatastoreServer") != std::string::npos) // if className contains DatastoreServer return "SERVER"
        return "SERVER";
    else if (std::string(className).find("Client") != std::string::npos) // if className contains Client return "CLIENT"
        return "CLIENT";
    else
        return "UNKNOWN"; // Default case if neither SERVER nor CLIENT is found
}

void DatastoreServer::sendServerCheckPartition(cMessage* msg , const char* gateName, int gateIndex){
    if(activeNetworkPartition && isChannelAffectedByNetworkPartition[gateIndex]){
            EV_INFO << "[SERVER-" << serverId << "] Outgoing Packet Not Sent | Cause: Network Partition | To: [SERVER-" << fromGateToServer(gateIndex) << "] | Type: " << msg->getClassName() << endl;
            delete msg;
            return;
    }
    send(msg, gateName, gateIndex);
}


