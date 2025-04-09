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
        int numClientsPerServer;
        double messageDelay @unit(s) = default(uniform(0.1s,0.5s));
        double omissionFailureProbability = default(0.05);
        double heartbeatInterval @unit(s) = messageDelay * 10;
        double retransmissionInterval @unit(s) = 0.5;
        double maxAllowedHeartbeatDelay @unit(s) = heartbeatInterval * 3;
        
    gates:
        inout serverChannels[totalServers-1];
        inout clientChannels[numClientsPerServer];
}

simple Client{
	parameters:
	    int clientId;
	    int connectedServerId;
	    double operationInterval @unit(s) = default(exponential(1s));
	    double readProbability = default(0.8);
        string keyspace = default("keyA,keyB,keyC");
	    
	gates:
        inout serverChannel;
}

network CausalConsistencyDatastoreNetwork{
    parameters:
        int numServers = default(3);
        int numClientsPerServer = default(2);
        
    submodules:
        servers[numServers]: DatastoreServer {
            parameters:
                serverId = index;
                totalServers = parent.numServers;
                numClientsPerServer = parent.numClientsPerServer;
            gates:
                //Standard Topology: every Datastore Server is connected to everyone else (Dense Graph)
                serverChannels[parent.numServers-1];
        }
        
        clients[numServers * numClientsPerServer]: Client{
            parameters:
                clientId = index;
                connectedServerId = int(index / parent.numClientsPerServer);
        }
        
    connections:
        //Servers Connection
        for i=0..numServers - 2, for j=i+1..numServers-1 {
            servers[i].serverChannels[j-1] <--> { delay = default(uniform(0.1s, 0.5s)); } <--> servers[j].serverChannels[i];
        }
        
        //Clients Connection
        for i=0..numServers * numClientsPerServer - 1 {
            clients[i].serverChannel <--> {delay = default(uniform(0.01s,0.1s));} <--> servers[clients[i].connectedServerId].clientChannels[i % numClientsPerServer];
        }
}
