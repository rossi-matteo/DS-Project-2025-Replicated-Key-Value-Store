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
    operationTimer = nullptr;
}

Client::~Client() {}

void Client::initialize() {
    clientId = par("clientId");
    connectedServerId = par("connectedServerId");
    operationInterval = par("operationInterval");
    readProbability = par("readProbability");

    numReadsPerformed = 0;
    numWritesPerformed = 0;
    numFailedOperations = 0;
    totalOperationTime = 0;
    readLatencyStats.setName("Read Latency");
    writeLatencyStats.setName("Write Latency");

    currentOperation = OP_NONE;

    operationTimer = new cMessage("operationTimer");
    scheduleAt(simTime() + operationInterval, operationTimer);

    std::stringstream ss(par("keyspace").stringValue());
    std::string key;
    while(std::getline(ss, key, ',')){
        keySpace.push_back(key);
    }
    
    EV_INFO << "[CLIENT-" << clientId << "] Starting | Connected To: [SERVER-" << connectedServerId << "]" << endl;
}

void Client::finish() {
    EV_INFO << "[CLIENT-" << clientId << "] Terminating |" << endl;
    EV_INFO << "Reads performed: " << numReadsPerformed << endl;
    EV_INFO << "Writes performed: " << numWritesPerformed << endl;
    EV_INFO << "Failed operations: " << numFailedOperations << endl;
    EV_INFO << "Total operation time: " << totalOperationTime << endl;
    if (numReadsPerformed > 0){
        EV_INFO << "Read Latency: " << readLatencyStats.getMean() << endl;
    }
    if (numWritesPerformed > 0){
        EV_INFO << "Write Latency: " << writeLatencyStats.getMean() << endl;
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
    
    NetworkMsg *inboundMsg = check_and_cast<NetworkMsg *>(msg);

    if(dynamic_cast<ReadResponseMsg *>(inboundMsg)){
        ReadResponseMsg *readResponse = dynamic_cast<ReadResponseMsg *>(inboundMsg);
        if(currentOperation == OP_READ && readResponse -> getKey() == currentKey){
            currentOperation = OP_NONE;
            simtime_t latency = simTime() - currentOperationStartTime;
            readLatencyStats.collect(latency);
            totalOperationTime += latency.dbl(); // Accumulate operation time
            EV_INFO << "[CLIENT-" << clientId << "] Read Reply | <" << readResponse -> getKey() << ", " << readResponse -> getValue() << "> | Latency: " << latency << endl;
        }
    } else if(dynamic_cast<WriteResponseMsg *>(inboundMsg)){
        WriteResponseMsg *writeResponse = dynamic_cast<WriteResponseMsg *>(inboundMsg);
        if(currentOperation == OP_WRITE && writeResponse -> getKey() == currentKey){
            currentOperation = OP_NONE;
            simtime_t latency = simTime() - currentOperationStartTime;
            writeLatencyStats.collect(latency);
            totalOperationTime += latency.dbl(); // Accumulate operation time
            EV_INFO << "[CLIENT-" << clientId << "] Write Reply | <" << writeResponse -> getKey() << "> | To: [SERVER-" << connectedServerId << "] | Latency: " << latency << endl;
        }
    }

    delete inboundMsg;
}

void Client::performOperation() {
    if (currentOperation != OP_NONE){
        EV_INFO << "[CLIENT-" << clientId << "] Operation Not Performed In Time | (By the server - Message Lost or Slow)" << endl;
        numFailedOperations++; // Increment failed operations
        currentOperation = OP_NONE;
        return;
    }

    double random = uniform(0, 1);
    if(random < readProbability){
        sendRead();
    } else {
        sendWrite();
    }
}

void Client::sendRead(){
    std::string key = generateKey();

    ReadRequestMsg *readRequestMsg = new ReadRequestMsg();
    readRequestMsg -> setSourceId(clientId);
    readRequestMsg -> setKey(key.c_str());

    send(readRequestMsg, "serverChannel$o");

    currentOperation = OP_READ;
    currentKey = key;
    currentOperationStartTime = simTime();
    numReadsPerformed++;

    EV_INFO << "[CLIENT-" << clientId << "] Read Request | <" << key << ">" << endl;
}

void Client::sendWrite(){
    std::string key = generateKey();
    int value = generateValue();

    WriteRequestMsg *writeRequestMsg = new WriteRequestMsg();
    writeRequestMsg -> setSourceId(clientId);
    writeRequestMsg -> setKey(key.c_str());
    writeRequestMsg -> setValue(value);

    send(writeRequestMsg, "serverChannel$o");

    currentOperation = OP_WRITE;
    currentKey = key;
    currentOperationStartTime = simTime();
    numWritesPerformed++;

    EV_INFO << "[CLIENT-" << clientId << "] Write Request | <" << key << ", " << value << ">" << endl;
}

std::string Client::generateKey(){
    int randomIndex = uniform(0, keySpace.size());
    return keySpace[randomIndex];
}

int Client::generateValue(){
    return uniform(0, 1000);
}
