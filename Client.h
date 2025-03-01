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

#ifndef CLIENT_H_
#define CLIENT_H_

class Client {
    private:
        int clientId;
        int connectedServerId;

        double operationInterval;
        double readProbability;

        std::vector<std::string> keySpace;

        /*
        Statistics
        int numReadsPerformed;
        int numWritesPerformed;
        cHistogram readLatencyStats;    
        cHistogram writeLatencyStats;
        */

        typedef enum {OP_READ, OP_WRITE, OP_NONE} OperationType;
        OperationType currentOperation;
        std::string currentKey;
        simtime_t operationStartTime;

        cMessage *operationTimer;


    public:
        Client();
        virtual ~Client();

        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;

        void sendRead();
        void sendWrite();

        void performOperation();
};

#endif /* CLIENT_H_ */

Define_Module(Client);
