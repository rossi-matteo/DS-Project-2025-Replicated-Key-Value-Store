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

#include "Client.h"

Client::Client() {
    operationTimer = null;
}

Client::~Client() {}

void Client::initialize() {
    clientId = par("clientId");
    connectedServerId = par("connectedServerId");
    operationInterval = par("operationInterval");
    readProbability = par("readProbability");

    /*
    numReadsPerformed = 0;
    numWritesPerformed = 0;
    readLatencyStats.setName("Read latency");
    writeLatencyStats.setName("Write latency");
    */

    currentOperation = OP_NONE;

    operationTimer = new cMessage("operationTimer");
    scheduleAt(simTime() + operationInterval, operationTimer);

    std::stringstream ss(par("keyspace").stringValue());
    std::string key;
    while(std::getline(ss, key, ',')){
        keySpace.push_back(key);
    }
    
    EV << "Client " << clientId << " initialized - Connected to " << connectedServerId << endl;
}

void Client::finish() {
    EV << "Client " << clientId << " finished" << endl;
    //EV << "Reads performed: " << numReadsPerformed << endl;
    //EV << "Writes performed: " << numWritesPerformed << endl;
    if (numReadsPerformed > 0){
        //EV << "Read latency stats: " << readLatencyStats.getStatistics() << endl;
    }
    if (numWritesPerformed > 0){
        //EV << "Write latency stats: " << writeLatencyStats.getStatistics() << endl;
    }
}

void Client::handleMessage(cMessage *msg) {

    if(msg -> isSelfMessage()){
        if(msg == operationTimer) {
            performOperation();
            scheduleAt(simTime() + operationInterval, operationTimer);
        }
        return;
    }
    
    NetworkMessages *inboundMsg = check_and_cast<cMessage *>(msg);

    if(dynamic_cast<ReadResponseMsg *>(inboundMsg)){
        ReadResponseMsg *readResponse = dynamic_cast<ReadResponseMsg *>(inboundMsg);
        if(readResponse -> getOperationId() == currentOperationId){
            currentOperation = OP_NONE;
            //numReadsPerformed++;
            simtime_t latency = simTime() - currentOperationStartTime;
            //readLatencyStats.collect(latency);

            EV << "Client " << clientId << " received value " << readResponse -> getValue() << " for key " << readResponse -> getKey() << ", latency: " << latency << endl;
        }
    } else if(dynamic_cast<WriteResponseMsg *>(inboundMsg)){
        WriteResponseMsg *writeResponse = dynamic_cast<WriteResponseMsg *>(inboundMsg);
        if(writeResponse -> getOperationId() == currentOperationId){
            currentOperation = OP_NONE;
            //numWritesPerformed++;
            //writeLatencyStats.collect(simTime() - currentOperationStartTime);
            simtime_t latency = simTime() - currentOperationStartTime;
            EV << "Client " << clientId << " wrote value " << writeResponse -> getValue() << " for key " << writeResponse -> getKey() << ", latency: " << latency << endl;
        }
    }

    delete inboundMsg;
}

void Client::performOperation() {
    if (currentOperation != OP_NONE){
        EV << "Client " << clientId << " is already performing an operation" << endl;
        return;
    }

    double random = uniform(0, 1);
    if(random < readProbability){
        sendRead();
    } else {
        sendWrite();
    }
}

void Client::read(){
    std::string key = generateKey();

    ReadRequestMsg *readRequestMsg = new ReadRequestMsg();
    readRequestMsg.setSourceId(clientId);
    readRequestMsg.setKey(key.c_str());

    send(readRequestMsg, "outputChannel");

    currentOperation = OP_READ;
    currentKey = key;
    currentOperationStartTime = simTime();
    //numReadsPerformed++;

    EV << "Client " << clientId << " sent read request for key: " << key << endl;
}

void Client::write(){
    std::string key = generateKey();
    int value = generateValue();

    WriteRequestMsg *writeRequestMsg = new WriteRequestMsg();
    writeRequestMsg.setSourceId(clientId);
    writeRequestMsg.setKey(key.c_str());
    writeRequestMsg.setValue(value);

    send(writeRequestMsg, "outputChannel");

    currentOperation = OP_WRITE;
    currentKey = key;
    currentOperationStartTime = simTime();
    //numWritesPerformed++;

    EV << "Client " << clientId << " sent write request for <" << key << ", " << value << ">" << endl;
}

std::string Client::generateKey(){
    int randomIndex = uniform(0, keySpace.size() - 1);
    return keySpace[randomIndex];
}

int Client::generateValue(){
    return uniform(0, 1000);
}
