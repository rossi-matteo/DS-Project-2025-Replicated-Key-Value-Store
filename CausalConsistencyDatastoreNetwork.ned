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

package ds_replicated_datastore;

simple DatastoreServer{
    parameters:
        int serverId;
        int totalServers;
        double messageDelay @unit(s) = default(uniform(0.1s,0.5s));
        double omissionFailureProbability = default(0.05);
        
    gates:
        input inputChannel[n];
        output outputChannel[n];
}

simple Client{
	parameters:
	    int clientId;
	    int connectedServerId;
	    double operationInterval @unit(s) = default(exponential(1s));
	    double readProbability = default(0.8);
	    
	gates:
	    input inputChannel;
	    output outputChannel;    
}

network CausalConsistencyDatastoreNetwork{
    parameters:
        int numServers = default(3);
        int numClientPerServer = default(2);
        
    submodules:
        servers[numServers]: DatastoreServer {
            parameters:
                serverId = index;
                totalServers = numServers;
            gates:
                //Standard Topology: every Datastore Server is connected to everyone else (Dense Graph)
                inputChannel[numServers-1];
                outputChannel[numServers-1];
        }
        
        clients[numServers*numClientsPerServer]: Client{
            parameters:
                clientId = index;
                connectedServerId = index / numClientsPerServer;
        }
        
    connections:
        for i=0..numServers-1, for j=0..numServers-1, if i != j {
            servers[i].outputChannel[j > i ? j-1 : j] --> { delay = uniform(0.1s, 0.5s); } --> servers[j].inputChannel[i > j ? i-1 : i];
        }
        
        //Clients Connection
        for i=0..numServers*numClientsPerServer-1 {
            clients[i].outputChannel --> {delay = default(uniform(0.01s,0.1s));} --> servers[client[i].connectedServerId].inputChannel[numServers-1+i];
            servers[clients[i].connectedServerId].outputChannel[numServers-1+i] --> {delay = default(uniform(0.01s,0.1s));} --> clients[i].inputChannel;
        }
}


