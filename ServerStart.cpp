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

bool isToAllCommand(string_view message) {
    return message.find(TOALL) == 0;
}

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
    string_view id = rest.substr(0, pos); // 987987987::Привет => 987987987 
    return string(id);
}

// DIRECT::user_id::message_text
// DIRECT::1::Привет => 1
string parseDirectMessage(string_view message) {
    // DIRECT::1212::Привет => 1212::Привет 
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");
    string_view text = rest.substr(pos + 2); // 987987987::Привет => Привет
    return string(text);
}

// Возвращает true если это команда установления имени
bool isDirectCommand(string_view message) {
    // "SET_NAME:: Mike"->find("SET_NAME::") == 0;
    // "SET_NAME:: Mike"->find("Mike") == 10;
    return message.find(DIRECT) == 0;
}

// ONLINE::1::Mike
// OFFLINE::1::Mike
string makeOnline(int user_id, string user_name) {
    return "ONLINE::" + to_string(user_id) + "::" + user_name;
}

string makeOffline(int user_id, string user_name) {
    return "OFFLINE::" + to_string(user_id) + "::" + user_name;
}
    
int main() {
     // Структура данных, которая хранит всю инормацию об одном пользователе чата
    struct PerSocketData {
        int user_id;
        string name;
    };

    int last_user_id = 10;
    map<int, string> usersOnline; // имена и id пользователей, которые онлайн

     // Создаём новый сервер
    uWS::App() // Создаём новый сервер
        .ws<PerSocketData>("/*", // Указываем, что будем хранить инфу о каждом подключившемся в PerSocketData struct
        { 
            /* Settings */
            // Настройки сетевой части
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 600, // Таймаут молчания(выкидывает по истечении бездействия)
            .maxBackpressure = 1 * 1024 * 1024,

            /* Handlers (Обработчики)*/
            .upgrade = nullptr,
            .open = [&last_user_id, &usersOnline](auto* connection) {
                // При открытии соединения
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // Назначить уникальный номер
                userData->user_id = last_user_id++;
                // Добавить пустое имя
                userData->name = "UNNAMED";
                usersOnline[userData->user_id] = userData->name;
                connection->subscribe(BROAD_CHANNEL); // Подписываем на канал "Широкое вещание"(имя любое)
                connection->subscribe("user#" + to_string(userData->user_id));
                /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
                // cout << "New connection created \n";
            },
            .message = [&usersOnline](auto* connection, string_view message, uWS::OpCode opCode) {
                cout << "New message received" << message << "\n";
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // 1. Сообщает своё имя пользователь
                if (isSetNameCommand(message)) {
                    cout << "User set their name \n";
                    userData->name = parseName(message);
                    // Нужно записать имя
                    usersOnline[userData->user_id] = userData->name;
                    // Сообщить всем о подключении пользователя
                    connection->publish(BROAD_CHANNEL, makeOnline(userData->user_id, userData->name));
                    // Сообщить пользователю обо всех подключениях
                    for (auto entry : usersOnline) {
                        //entry.first = id userData->user_id
                        //entry.second == name userData->name
                        connection->send(makeOnline(entry.first, entry.second), uWS::OpCode::TEXT);
                        //connection->publish(
                        //    "user#" + to_string(userData->user_id), // Равноценно connection->send(makeOnline(userData->user_id, userData->name));
                        //    makeOnline(userData->user_id, userData->name)
                        //);
                        
                    }
                }
                // 2. Может кому-то написать
                if (isDirectCommand(message)) {
                    cout << "User sent direct message \n";
                    // Нужно переправить сообщение получателю
                    string id = parseReceiverId(message);
                    string text = parseDirectMessage(message);
                    connection->publish("user#" + id, "DIRECT::" + userData->name + ":: " + text); // Отправляем получателю текст сообщения
                    // В сложных серваках записывается в БД
                }
                // 3. Сообщение в общий чат
                if (isToAllCommand(message)) {
                    string text = parseToAllText(message);
                    connection->publish(BROAD_CHANNEL, "TOALL::" + userData->name + "::" + text); // Отправляем текст всем
                }
            },
            .close = [&usersOnline](auto* connection, int /*code*/, std::string_view /*message*/) {
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                cout << "Connection closed";
                // Забыть пользователя
                connection->publish(BROAD_CHANNEL, makeOffline(userData->user_id, userData->name));
                // При отключении пользователя от сервера
                // Сообщить всем об отключении пользователя

                usersOnline.erase(userData->user_id); // делит с карты
            }
                // Запуск сервера
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}