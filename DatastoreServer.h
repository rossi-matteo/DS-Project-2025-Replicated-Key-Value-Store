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

using namespace omnetpp;

#ifndef DATASTORESERVER_H_
#define DATASTORESERVER_H_

class DatastoreServer {
    private:
        int serverId;
        int totalServers;

        //Key-Value Store
        std::map<std::string, int> store;

        //Vector Clocks
        std::vector<int> vectorClock;

        //Pending Updates
        typedef struct {
            int updateId; //Maybe useless
            int sourceId;

            std::vector<int> senderVectorClock;

            std::string key;
            int value;
        }PendingUpdate;

        std::vector<PendingUpdate> pendingUpdates;

        /*
         * std::vector<bool>areServersUp;
         * */

        int updateIdCounter;

        //cMessage *heartbeatTimer;

    public:
        DatastoreServer();
        virtual ~DatastoreServer();

        //Standard Omnet++ methods to Override
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;

        void handleRead(ReadRequestMsg *msg);
        void handleWrite(WriteRequestMsg *msg);
        void handleUpdate(UpdateRequestMsg *msg);

        void sendUpdate(std::string key, int value);

        void applyUpdate();
        void checkPendingUpdates();

        bool checkCausalDependencies();

        //TODO
        /*
         * void handleHeartbeat(HeartbeatMsg *msg);
         * void sendHeartbeat();
         *
         * */
};

#endif /* DATASTORESERVER_H_ */

Define_Module(DatastoreServer);
