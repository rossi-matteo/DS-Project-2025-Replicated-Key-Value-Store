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
#include "NetworkMessages_m.h"

using namespace omnetpp;

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

        // Message used to periodically send heartbeats to other datastores
        cMessage *heartbeatTimer;

        // Used to check updates applied by every datastore (to delete them from the receivedUpdates map)
        std::map<int, std::map<int,int>> lastKnownVectorClocks;

        // Vector clock containing as fields the youngest update (for each server) received (not necessarily applied) by everyone
        std::map<int, int> prevMinVectorClock;

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
};

#endif /* DATASTORESERVER_H_ */

Define_Module(DatastoreServer);
