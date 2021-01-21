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


const string SET_NAME = "SET_NAME::";
const string DIRECT = "DIRECT::";
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
    string_view id = rest.substr(0, pos); // 987987987::������ => 987987987 
    return string(id);
}

// DIRECT::user_id::message_text
// DIRECT::1::������ => 1
string parseDirectMessage(string_view message) {
    // DIRECT::1212::������ => 1212::������ 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");
    string_view text = rest.substr(pos + 2); // 987987987::������ => ������
    return string(text);
}

// ���������� true ���� ��� ������� ������������ �����
bool isDirectCommand(string_view message) {
    // "SET_NAME:: Mike"->find("SET_NAME::") == 0;
    // "SET_NAME:: Mike"->find("Mike") == 10;
    return message.find(DIRECT) == 0;
}
    
int main() {
     // ��������� ������, ������� ������ ��� ��������� �� ����� ������������ ����
    struct PerSocketData {
        int user_id;
        string name;
    };

    int last_user_id = 10;

     // ������ ����� ������
    uWS::App() // ������ ����� ������
        .ws<PerSocketData>("/*", // ���������, ��� ����� ������� ���� � ������ �������������� � PerSocketData struct
        { 
            /* Settings */
            // ��������� ������� �����
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 600, // ������� ��������(���������� �� ��������� �����������)
            .maxBackpressure = 1 * 1024 * 1024,

            /* Handlers (�����������)*/
            .upgrade = nullptr,
            .open = [&last_user_id](auto* connection) {
                // ��� �������� ����������
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // ��������� ���������� �����
                userData->user_id = last_user_id++;
                // �������� ������ ���
                userData->name = "UNNAMED";

                connection->subscribe("broadcast"); // ����������� �� ����� "������� �������"(��� �����)
                connection->subscribe("user#" + to_string(userData->user_id));
                /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
                // cout << "New connection created \n";
            },
            .message = [](auto* connection, string_view message, uWS::OpCode opCode) {
                cout << "New message received" << message << "\n";
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // 1. �������� ��� ��� ������������
                if (isSetNameCommand(message)) {
                    cout << "User set their name \n";
                    userData->name = parseName(message);
                    // ����� �������� ���
                }
                // 2. ����� ����-�� ��������
                if (isDirectCommand(message)) {
                    cout << "User sent direct message \n";
                    // ����� ����������� ��������� ����������
                    string id = parseReceiverId(message);
                    string text = parseDirectMessage(message);
                    connection->publish("user#" + id, text); // ���������� ���������� ����� ���������
                    // � ������� �������� ������������ � ��
                }
                // ��� ��������� ��������� �� ������������
                // ws->send(message, opCode, true);
            },
            .close = [](auto*/*ws*/, int /*code*/, std::string_view /*message*/) {
                cout << "Connection closed";
                // ������ ������������
                // ��� ���������� ������������ �� �������
                /* You may access ws->getUserData() here */
            }
                // ������ �������
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}