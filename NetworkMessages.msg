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

cplusplus {{
#include <map>
#include <set>
#include <string>
typedef std::map<int, int> VectorClock;
typedef std::map<std::string, int> Store;
typedef std::map<int, std::set<int>> Writes;
}}

class VectorClock {@existingClass;}
class Store {@existingClass;}
class Writes{@existingClass;}

message NetworkMsg {
    int sourceId;
}

message ReadRequestMsg extends NetworkMsg {
    string key;
}

message ReadResponseMsg extends NetworkMsg{
    string key;
    int value;
}

message WriteRequestMsg extends NetworkMsg {
    string key;
    int value;
}

message WriteResponseMsg extends NetworkMsg{
    string key;
}

message WritePropagationMsg extends NetworkMsg {
    string key;
    int value;
    VectorClock vectorClock;
}

message HeartbeatMsg extends NetworkMsg{
    simtime_t timestamp;
    VectorClock vectorClock;
}

message MissingWritesRequestMsg extends NetworkMsg {
    Writes missingWrites;
}








