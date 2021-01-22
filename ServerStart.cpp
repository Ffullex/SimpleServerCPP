#include <iostream>
#include <uwebsockets/App.h>

using namespace std;

// �������� ������� � ��������
// 1. ������������ �������� ��� ���
// SET_NAME:: Mike

// 2. DIRECT::user_id::message_text
// DIRECT::1::������, ����
// DIRECT::2::� ���� ������, ����

// 3. �����������/����������
// ONLINE::1::Mike
// OFFLINE::1::Mike

// 4. ����� ���.
// TOALL::������ ����!

const string BROAD_CHANNEL = "broadcast";
const string SET_NAME = "SET_NAME::";
const string DIRECT = "DIRECT::";
const string TOALL = "TOALL::";

bool isToAllCommand(string_view message);
string parseToAllText(string_view message);
bool isSetNameCommand(string_view message);
string parseName(string_view message);
string parseReceiverId(string_view message);
bool isDirectCommand(string_view message);
string parseDirectMessage(string_view message);
string makeOnline(int user_id, string user_name);
string makeOffline(int user_id, string user_name);

int main() {
    // ��������� ������, ������� ������ ��� ��������� �� ����� ������������ ����.
    struct PerSocketData {
        int user_id;
        string name;
    };

    // ����������, ������� ������ ID ������������.
    int last_user_id = 10;

    // ����� � id �������������, ������� ������.
    map<int, string> usersOnline; 

// ������ ����� ������.
    uWS::App() 
        // ���������, ��� ����� ������� ���� � ������ �������������� � PerSocketData struct.
        .ws<PerSocketData>("/*", 
        { 
            // ��������� ������� �����.
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 600, // ������� ��������(���������� �� ��������� �����������).
            .maxBackpressure = 1 * 1024 * 1024,

            // Handlers (�����������):
            .upgrade = nullptr,

            // ���������� ��� �������� ����������.
            .open = [&last_user_id, &usersOnline](auto* connection) {
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                
                // ��������� ���������� �����
                userData->user_id = last_user_id++;
                
                // �������� ������ ���
                userData->name = "UNNAMED";
                usersOnline[userData->user_id] = userData->name;

                // ����������� �� ����� "broadcast"(��� �����)
                connection->subscribe(BROAD_CHANNEL); 

                connection->subscribe("user#" + to_string(userData->user_id));
                cout << "New connection created \n";
            },
            
            // ���������� ���������.
            .message = [&usersOnline](auto* connection, string_view message, uWS::OpCode opCode) {
                cout << "New message received" << message << "\n";
                PerSocketData* userData = (PerSocketData*)connection->getUserData();

                // 1. ������������ �������� ��� ��� � �������� ����������� ����� �����
                if (isSetNameCommand(message)) {
                    cout << "User set their name \n";
                    userData->name = parseName(message);

                    // ����� �������� ���
                    usersOnline[userData->user_id] = userData->name;

                    // �������� ���� � ����������� ������������
                    connection->publish(BROAD_CHANNEL, makeOnline(userData->user_id, userData->name));

                    // �������� ������������ ��� ���� ������������
                    for (auto entry : usersOnline) {

                        //entry.first = userData->user_id
                        //entry.second = userData->name
                        connection->send(makeOnline(entry.first, entry.second), uWS::OpCode::TEXT);

                        //connection->publish("user#" + to_string(userData->user_id), makeOnline(userData->user_id, userData->name));
                        // ���������� connection->send(makeOnline(userData->user_id, userData->name));
                    }
                }

                // 2. ��������� ������
                if (isDirectCommand(message)) {
                    cout << "User sent direct message \n";
                    // ����� ����������� ��������� ����������
                    string id = parseReceiverId(message);
                    string text = parseDirectMessage(message);

                    // ���������� ���������� ����� ���������
                    connection->publish("user#" + id, DIRECT + userData->name + ":: " + text); 
                    // � ������� �������� ������������ � ��
                }

                // 3. ��������� � ����� ���
                if (isToAllCommand(message)) {
                    string text = parseToAllText(message);

                    // ����������� ��������� � ����� ���
                    connection->publish(BROAD_CHANNEL, TOALL + userData->name + "::" + text); 
                }
            },

            // ���������� ��� ���������� ����������.
            .close = [&usersOnline](auto* connection, int /*code*/, std::string_view /*message*/) {
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
              
                // �������� ���� �� ���������� ������������.
                cout << "Connection closed";

                // ������ ������������.
                connection->publish(BROAD_CHANNEL, makeOffline(userData->user_id, userData->name));

                // ������� � �����.
                usersOnline.erase(userData->user_id); 
            }
               
            // ������ �������, ����� �����
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}

// ���������� true ���� ��� ��������� ��� ����
bool isToAllCommand(string_view message) {
    return message.find(TOALL) == 0;
}

// TOALL::������! => ������!
string parseToAllText(string_view message) {
    return string(message.substr(TOALL.length()));
}

// ���������� true ���� ��� ������� ������������ �����
bool isSetNameCommand(string_view message) {
    // "SET_NAME:: Mike"->find("SET_NAME::") == 0;
    // "SET_NAME:: Mike"->find("Mike") == 10;
    return message.find(SET_NAME) == 0;
}

// SET_NAME::Mike => Mike
string parseName(string_view message) {
    return string(message.substr(SET_NAME.length())); // ���������� ���
}

// DIRECT::user_id::message_text
// DIRECT::1::������ => 1
string parseReceiverId(string_view message) {

    // DIRECT::1::������ => 1::������ 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");

    // 987987987::������ => 987987987 
    string_view id = rest.substr(0, pos); 
    return string(id);
}

// DIRECT::user_id::message_text
// DIRECT::1::������ => 1
string parseDirectMessage(string_view message) {

    // DIRECT::1212::������ => 1212::������ 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");

    // 987987987::������ => ������
    string_view text = rest.substr(pos + 2); 
    return string(text);
}

// ���������� true ���� ��� ������� ������������ �����
bool isDirectCommand(string_view message) {

    // "SET_NAME:: Mike"->find("SET_NAME::") == 0;
    // "SET_NAME:: Mike"->find("Mike") == 10;
    return message.find(DIRECT) == 0;
}

// ONLINE::1::Mike
string makeOnline(int user_id, string user_name) {
    return "ONLINE::" + to_string(user_id) + "::" + user_name;
}

// OFFLINE::1::Mike
string makeOffline(int user_id, string user_name) {
    return "OFFLINE::" + to_string(user_id) + "::" + user_name;
}
