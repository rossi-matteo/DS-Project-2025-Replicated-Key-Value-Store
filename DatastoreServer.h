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

#include <omnetpp.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <random>
#include "NetworkMessages_m.h"

using namespace omnetpp;

// Message Stats Tracking Struct
struct MessageStats{
    // Outgoing messages by type
    std::map<std::string, int> outgoingByType;
    
    // Incoming messages by type
    std::map<std::string, int> incomingByType;
    
    // Discarded messages (Omission/Network Partition) by type
    std::map<std::string, int> discardedByType;

    //Tracking how many partitions started from this server
    int partitionsStarted; // Number of partitions started from this server
    
    // Specific tracking for MissingWrites requests
    int missingWritesUpdatesRequested;       // Total missing writes requested
    int missingWritesUpdatesSkippedCooldown; // Missing writes not requested due to cooldown
    
    int missingWritesFulfilled;       // Missing writes successfully sent back
    
    // Add or increment a message count by type
    void incrementOutgoing(const std::string& msgType) {
        outgoingByType[msgType]++;
    }
    
    void incrementIncoming(const std::string& msgType) {
        incomingByType[msgType]++;
    }
    
    void incrementDiscarded(const std::string& msgType) {
        discardedByType[msgType]++;
    }
    
    // Methods to record statistics in OMNeT++ format
    void recordScalar(cComponent* component, const std::string& prefix) const {
        // Record outgoing messages
        for (const auto& [type, count] : outgoingByType) {
            component->recordScalar((prefix + "outgoing_" + type).c_str(), count);
        }
        
        // Record Incoming messages
        for (const auto& [type, count] : incomingByType) {
            component->recordScalar((prefix + "incoming_" + type).c_str(), count);
        }
        
        // Record Discarded Messages (Omission/Network Partition)
        for (const auto& [type, count] : discardedByType) {
            component->recordScalar((prefix + "discarded_" + type).c_str(), count);
        }
        
        // Record MissingWrites statistics
        component->recordScalar((prefix + "partitions_started").c_str(), partitionsStarted);
        
        component->recordScalar((prefix + "missing_writes_updates_requested").c_str(), missingWritesUpdatesRequested);
        component->recordScalar((prefix + "missing_writes_updates_skipped_cooldown").c_str(), missingWritesUpdatesSkippedCooldown);

        component->recordScalar((prefix + "missing_writes_msg_fulfilled").c_str(), missingWritesFulfilled);
    }
};   

#ifndef DATASTORESERVER_H_
#define DATASTORESERVER_H_

class DatastoreServer : public cSimpleModule {
    private:
        // ID associated to this datastore
        int serverId;

        // Total number of servers in the system
        int totalServers;

        // Probability to drop a incoming/outgoing packet
        double omissionFailureProbability;

        // Probability to create a network partition containing this datastore
        double createNetworkPartitionProbability;

        // Probability to terminate the network partition containing this datastore
        double terminateNetworkPartitionProbability;

        // State variable to check if the server is currently in a network partition
        bool activeNetworkPartition;

        // Probability to include a link in the network partition
        bool networkPartitionLinkProbability;

        // Key-Value Store
        std::map<std::string, int> store;

        // Vector clock associated to the datastore
        std::map<int,int> vectorClock;

        // Online datastores and the timestamp of their last heartbeat
        std::map<int, simtime_t> onlineDatastores;
        
        // Struct definition for storing update information
        typedef struct {
            int sourceId;
            std::map<int, int> senderVectorClock;
            std::string key;
            int value;
        } Update;

        // Updates which at the moment do not satisfy causal dependencies and will be applied later
        std::vector<Update> pendingUpdates;  

        // Log of updates (for updating other datastores when they loose messages)
        std::map<int, std::map<int, Update>> receivedUpdates;

        // Used to check updates applied by every datastore (to delete them from the receivedUpdates map)
        std::map<int, std::map<int,int>> lastKnownVectorClocks;

        // Vector clock containing as fields the youngest update (for each server) received (not necessarily applied) by everyone
        std::map<int, int> prevMinVectorClock;

        // Message used to periodically send heartbeats to other datastores
        cMessage *heartbeatTimer;

        // Message used to periodically create a partition on the network with an uniform probability
        cMessage *networkPartitionEventTimer;

        // Contains the status of a link during a partition
        bool *isChannelAffectedByNetworkPartition;

        // Tracking of Last Request made for a specific update
        typedef struct {
            simtime_t lastRequestTime;
            int requestCount;
        }RequestInfo;

        // Map to track when we last requested each missing update: <serverId, <updateId, RequestInfo>>
        std::map<int, std::map<int, RequestInfo>> lastRequestTimes;

        // Cooldown parameters
        const simtime_t INITIAL_COOLDOWN = 1.0; // 500ms initial cooldown
        const double BACKOFF_FACTOR = 2.0;     // Exponential Backoff multiplier
        const simtime_t MAX_COOLDOWN = 10.0;   // Maximum Cooldown (10 seconds) 

        struct MessageStats messageStats;

    public:
        DatastoreServer();
        virtual ~DatastoreServer();

        virtual void initialize();
        virtual void handleMessage(cMessage *msg);
        virtual void finish();

    private:
        void handleRead(ReadRequestMsg *msg);
        void handleWrite(WriteRequestMsg *msg);
        void handleUpdate(WritePropagationMsg *msg);
        void handleMissingWrites(MissingWritesRequestMsg *msg);
        void processMissingWrites(
            const std::map<int, int>& senderVectorClock,
            std::map<int, std::set<int>>& allMissingWrites,
            std::map<int, std::set<int>>& missingWritesToRequest,
            std::map<int, std::map<int, RequestInfo>>& lastRequestTimes,
            simtime_t currentTime
        );

        void sendMissingWritesRequest(std::map<int, std::set<int>>(missingWritesToRequest));

        void sendUpdate(std::string key, int value);

        void checkPendingUpdates();
        int isSatisfyingCausalDependencies(int sourceId, std::map<int, int> senderVectorClock);

        void handleHeartbeat(HeartbeatMsg *msg);
        void sendHeartbeats();

        void deleteUpdatesAppliedByEveryone();

        std::set<int> getOnlineDatastores();

        Update storeWrite(std::string key, int value, int serverId, std::map<int, int> vectorClock);

        inline int fromServerToGate(int id);
        inline int fromGateToServer(int index);
        std::string parseMessageToRetrieveSenderModuleClass(cMessage *msg);

        void sendServerCheckPartition(cMessage* msg , const char* gateName, int gateIndex);

        void logMessageStats();
};

#endif /* DATASTORESERVER_H_ */

Define_Module(DatastoreServer);
