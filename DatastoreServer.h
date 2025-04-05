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
#include "NetworkMessages_m.h"

using namespace omnetpp;

#ifndef DATASTORESERVER_H_
#define DATASTORESERVER_H_

class DatastoreServer : public cSimpleModule {
    private:
        int serverId;
        int totalServers;

        //Key-Value Store
        std::map<std::string, int> store;

        //Vector Clocks
        std::map<int,int> vectorClock;

        // Online Datastores vector
        std::unordered_set<int> onlineDatastores;

        //Pending Updates
        typedef struct {
            int updateId; //Maybe useless
            int sourceId;

            std::map<int, int> senderVectorClock;

            std::string key;
            int value;
        }PendingUpdate;

        std::vector<PendingUpdate> pendingUpdates;

        /*
         * std::vector<bool>areServersUp;
         * */

        int updateIdCounter;

        cMessage *heartbeatTimer;

    public:
        DatastoreServer();
        virtual ~DatastoreServer();

        virtual void initialize();
        virtual void handleMessage(cMessage *msg);
        virtual void finish();

        void handleRead(ReadRequestMsg *msg);
        void handleWrite(WriteRequestMsg *msg);
        void handleUpdate(UpdateMsg *msg);

        void sendUpdate(std::string key, int value);

        void applyUpdate(int sourceId, std::string key, int value);
        void checkPendingUpdates();

        bool isSatisfyingCausalDependencies(int sourceId, std::map<int, int> senderVectorClock);

        void handleHeartbeat(HeartbeatMsg *msg);
        void sendHeartbeats();
};

#endif /* DATASTORESERVER_H_ */

Define_Module(DatastoreServer);
