#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WIN32_WINNT 0x0600

#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
const int MAX_CLIENTS = 10;  // Максимальное количество подключенных клиентов
int currentClients = 0;       // Текущее количество клиентов
CRITICAL_SECTION cs;          // Критическая секция

struct BinaryRequest {
    char surname[64];
    double height;
    double weight;
};

struct BinaryResponse {
    char surname[64];
    char result[84];
    int status;  // 0 - норма, 1 - выше, 2 - ниже
};

string checkWeight(double height, double weight) {
    double idealWeight = height - 110;

    if (weight > idealWeight + 5) {
        return "Weight uppered";
    }
    else if (weight < idealWeight - 5) {
        return "Weight lowered";
    }
    else {
        return "Normal lvl";
    }
}

void handleClient(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;
    BinaryRequest request;
    BinaryResponse response;

    // Устанавливаем таймауты для сокета
    int timeout = 30000; // 30 секунд
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    cout << "Клиент работает в бинарном режиме" << endl;

    try {
        while (true) {
            memset(&request, 0, sizeof(request));

            // Таймаут 1: Ожидание данных
            int bytesReceived = recv(clientSocket, (char*)&request, sizeof(request), 0);

            if (bytesReceived == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT) {
                    cout << "Таймаут ожидания данных" << endl;
                }
                else {
                    cout << "Ошибка приема данных: " << error << endl;
                }
                break;
            }

            if (bytesReceived == 0) {
                cout << "Клиент отключился" << endl;
                break;
            }

            // Проверка на неполные данные (защита от слишком больших/маленьких данных)
            if (bytesReceived != sizeof(request)) {
                cout << "Получены данные некорректного размера: " << bytesReceived << " байт" << endl;
                continue;
            }

            request.surname[49] = '\0'; // Гарантия завершения строки
            cout << "Получено: " << request.surname
                << ", рост: " << request.height << ", вес: " << request.weight << endl;

            // Формируем ответ
            string result = checkWeight(request.height, request.weight);

            memset(&response, 0, sizeof(response));
            strncpy_s(response.surname, request.surname, 49);
            strncpy_s(response.result, result.c_str(), 19);
            response.status = (result.find("Normal") != string::npos) ? 0 :
                (result.find("uppered") != string::npos) ? 1 : 2;

            // Таймаут 2: Отправка ответа
            if (send(clientSocket, (char*)&response, sizeof(response), 0) == SOCKET_ERROR) {
                cout << "Ошибка отправки ответа" << endl;
                break;
            }
            cout << "Отправлен ответ: " << response.result << endl;
        }
    }
    catch (...) {
        cout << "Ошибка при обработке клиента" << endl;
    }

    closesocket(clientSocket);

    // Уменьшаем счетчик клиентов в критической секции
    EnterCriticalSection(&cs);
    currentClients--;
    cout << "Клиент отключился. Текущих клиентов: " << currentClients << "/" << MAX_CLIENTS << endl;
    LeaveCriticalSection(&cs);
}

int main() {
    try {
        SetConsoleOutputCP(1251);
        SetConsoleCP(1251);
        setlocale(LC_ALL, "Russian");

        // Инициализация критической секции
        InitializeCriticalSection(&cs);

        // Инициализация Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cout << "Ошибка инициализации Winsock" << endl;
            return 1;
        }

        // Создание сокета
        SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            cout << "Ошибка создания сокета" << endl;
            WSACleanup();
            DeleteCriticalSection(&cs);
            return 1;
        }

        // Настройка адреса сервера
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(8888);

        // Привязка сокета к адресу
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cout << "Ошибка привязки сокета" << endl;
            closesocket(serverSocket);
            WSACleanup();
            DeleteCriticalSection(&cs);
            return 1;
        }

        // Начинаем прослушивание
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            cout << "Ошибка прослушивания" << endl;
            closesocket(serverSocket);
            WSACleanup();
            DeleteCriticalSection(&cs);
            return 1;
        }

        cout << "Сервер запущен на порту 8888 (бинарный режим)" << endl;
        cout << "Максимальное количество клиентов: " << MAX_CLIENTS << endl;
        cout << "Ожидание подключений..." << endl;

        while (true) {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);

            // Принимаем подключение
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);

            if (clientSocket == INVALID_SOCKET) {
                cout << "Ошибка принятия подключения" << endl;
                continue;
            }

            // Получаем IP клиента
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

            // Проверка на превышение лимита подключений (защита от DDoS)
            EnterCriticalSection(&cs);
            if (currentClients >= MAX_CLIENTS) {
                LeaveCriticalSection(&cs);
                cout << "Отказ в подключении " << clientIP << ": достигнут лимит клиентов" << endl;
                closesocket(clientSocket);
                continue;
            }

            // Увеличиваем счетчик клиентов
            currentClients++;
            cout << "Новый клиент подключился: " << clientIP << " (" << currentClients << "/" << MAX_CLIENTS << ")" << endl;
            LeaveCriticalSection(&cs);

            // Обрабатываем клиента в отдельном потоке
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handleClient, (LPVOID)clientSocket, 0, NULL);
        }

        closesocket(serverSocket);
        WSACleanup();
        DeleteCriticalSection(&cs);
    }
    catch (...) {
        cout << "Критическая ошибка сервера!" << endl;
        DeleteCriticalSection(&cs);
    }

    return 0;
}
