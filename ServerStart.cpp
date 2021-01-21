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


const string SET_NAME = "SET_NAME::";
const string DIRECT = "DIRECT::";
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
    
int main() {
     // Структура данных, которая хранит всю инормацию об одном пользователе чата
    struct PerSocketData {
        int user_id;
        string name;
    };

    int last_user_id = 10;

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
            .open = [&last_user_id](auto* connection) {
                // При открытии соединения
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // Назначить уникальный номер
                userData->user_id = last_user_id++;
                // Добавить пустое имя
                userData->name = "UNNAMED";

                connection->subscribe("broadcast"); // Подписываем на канал "Широкое вещание"(имя любое)
                connection->subscribe("user#" + to_string(userData->user_id));
                /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
                // cout << "New connection created \n";
            },
            .message = [](auto* connection, string_view message, uWS::OpCode opCode) {
                cout << "New message received" << message << "\n";
                PerSocketData* userData = (PerSocketData*)connection->getUserData();
                // 1. Сообщает своё имя пользователь
                if (isSetNameCommand(message)) {
                    cout << "User set their name \n";
                    userData->name = parseName(message);
                    // Нужно записать имя
                }
                // 2. Может кому-то написать
                if (isDirectCommand(message)) {
                    cout << "User sent direct message \n";
                    // Нужно переправить сообщение получателю
                    string id = parseReceiverId(message);
                    string text = parseDirectMessage(message);
                    connection->publish("user#" + id, text); // Отправляем получателю текст сообщения
                    // В сложных серваках записывается в БД
                }
                // При получении сообщения от пользователя
                // ws->send(message, opCode, true);
            },
            .close = [](auto*/*ws*/, int /*code*/, std::string_view /*message*/) {
                cout << "Connection closed";
                // Забыть пользователя
                // При отключении пользователя от сервера
                /* You may access ws->getUserData() here */
            }
                // Запуск сервера
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}