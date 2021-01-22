#include <iostream>
#include <uwebsockets/App.h>

using namespace std;

// Протокол общения с сервером
// 1. Пользователь сообщает своё имя
// SET_NAME:: Mike

// 2. DIRECT::user_id::message_text
// DIRECT::1::Привет, Майк
// DIRECT::2::И тебе привет, Джон

// 3. Подключение/отключение
// ONLINE::1::Mike
// OFFLINE::1::Mike

// 4. Общий чат.
// TOALL::Привет всем!

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
    // Структура данных, которая хранит всю инормацию об одном пользователе чата.
    struct PerSocketData {
        int user_id;
        string name;
    };

    // Переменная, которая хранит ID пользователя.
    int last_user_id = 10;

    // Имена и id пользователей, которые онлайн.
    map<int, string> usersOnline; 

// Создаём новый сервер.
    uWS::App() 
        // Указываем, что будем хранить инфу о каждом подключившемся в PerSocketData struct.
        .ws<PerSocketData>("/*", 
        { 
            // Настройки сетевой части.
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 600, // Таймаут молчания(выкидывает по истечении бездействия).
            .maxBackpressure = 1 * 1024 * 1024,

            // Handlers (Обработчики):
            .upgrade = nullptr,

            // Обработчик при открытии соединения.
            .open = [&last_user_id, &usersOnline](auto* connection) {
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                
                // Назначить уникальный номер
                userData->user_id = last_user_id++;
                
                // Добавить пустое имя
                userData->name = "UNNAMED";
                usersOnline[userData->user_id] = userData->name;

                // Подписываем на канал "broadcast"(имя любое)
                connection->subscribe(BROAD_CHANNEL); 

                connection->subscribe("user#" + to_string(userData->user_id));
                cout << "New connection created \n";
            },
            
            // Обработчик сообщения.
            .message = [&usersOnline](auto* connection, string_view message, uWS::OpCode opCode) {
                cout << "New message received" << message << "\n";
                PerSocketData* userData = (PerSocketData*)connection->getUserData();

                // 1. Пользователь сообщает своё имя и получает сообщённные ранее имена
                if (isSetNameCommand(message)) {
                    cout << "User set their name \n";
                    userData->name = parseName(message);

                    // Нужно записать имя
                    usersOnline[userData->user_id] = userData->name;

                    // Сообщить всем о подключении пользователя
                    connection->publish(BROAD_CHANNEL, makeOnline(userData->user_id, userData->name));

                    // Сообщить пользователю обо всех подключениях
                    for (auto entry : usersOnline) {

                        //entry.first = userData->user_id
                        //entry.second = userData->name
                        connection->send(makeOnline(entry.first, entry.second), uWS::OpCode::TEXT);

                        //connection->publish("user#" + to_string(userData->user_id), makeOnline(userData->user_id, userData->name));
                        // Равноценно connection->send(makeOnline(userData->user_id, userData->name));
                    }
                }

                // 2. Сообщение личное
                if (isDirectCommand(message)) {
                    cout << "User sent direct message \n";
                    // Нужно переправить сообщение получателю
                    string id = parseReceiverId(message);
                    string text = parseDirectMessage(message);

                    // Отправляем получателю текст сообщения
                    connection->publish("user#" + id, DIRECT + userData->name + ":: " + text); 
                    // В сложных серверах записывается в БД
                }

                // 3. Сообщение в общий чат
                if (isToAllCommand(message)) {
                    string text = parseToAllText(message);

                    // Отправление сообщений в общий чат
                    connection->publish(BROAD_CHANNEL, TOALL + userData->name + "::" + text); 
                }
            },

            // Обработчик при завершении соединения.
            .close = [&usersOnline](auto* connection, int /*code*/, std::string_view /*message*/) {
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
              
                // Сообщить всем об отключении пользователя.
                cout << "Connection closed";

                // Забыть пользователя.
                connection->publish(BROAD_CHANNEL, makeOffline(userData->user_id, userData->name));

                // Удаляет с карты.
                usersOnline.erase(userData->user_id); 
            }
               
            // Запуск сервера, выбор порта
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}

// Возвращает true если это сообщение для всех
bool isToAllCommand(string_view message) {
    return message.find(TOALL) == 0;
}

// TOALL::Привет! => Привет!
string parseToAllText(string_view message) {
    return string(message.substr(TOALL.length()));
}

// Возвращает true если это команда установления имени
bool isSetNameCommand(string_view message) {
    // "SET_NAME:: Mike"->find("SET_NAME::") == 0;
    // "SET_NAME:: Mike"->find("Mike") == 10;
    return message.find(SET_NAME) == 0;
}

// SET_NAME::Mike => Mike
string parseName(string_view message) {
    return string(message.substr(SET_NAME.length())); // Возвращает имя
}

// DIRECT::user_id::message_text
// DIRECT::1::Привет => 1
string parseReceiverId(string_view message) {

    // DIRECT::1::Привет => 1::Привет 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");

    // 987987987::Привет => 987987987 
    string_view id = rest.substr(0, pos); 
    return string(id);
}

// DIRECT::user_id::message_text
// DIRECT::1::Привет => 1
string parseDirectMessage(string_view message) {

    // DIRECT::1212::Привет => 1212::Привет 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");

    // 987987987::Привет => Привет
    string_view text = rest.substr(pos + 2); 
    return string(text);
}

// Возвращает true если это команда установления имени
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
