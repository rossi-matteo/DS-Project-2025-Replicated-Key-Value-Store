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
#include <string>
#include <vector>
typedef std::vector<int> VectorClock;
typedef std::map<std::string, int> Store;
}}

class noncobject VectorClock;
class noncobject Store;

packet NetworkMessages {
    int sourceId;
}

packet ReadRequestMsg extends NetworkMsg {
    string key;
}

packet WriteRequestMsg extends NetworkMsg {
    string key;
    int value;
}
