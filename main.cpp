#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WIN32_WINNT 0x0600

#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <sstream>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

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

void handleClient(SOCKET clientSocket) {//добавить реализацию для бинарников(на основе 2-х указателей в буфере потом)
    char buffer[1024];

    try {
        while (true) {
            // Получаем данные от клиента
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);//настроить таймауты(3 шт)

            if (bytesReceived <= 0) {
                cout << "Клиент отключился" << endl;
                break;
            }

            string data(buffer);
            cout << "Получено: " << data << endl;

            string surname;
            double height, weight;
            istringstream iss(data);

            if (iss >> surname >> height >> weight) {
                string result = checkWeight(height, weight);
                string response = surname + ": " + result + "\n";

                // Отправляем результат обратно клиенту
                send(clientSocket, response.c_str(), response.length(), 0);
                cout << "Отправлено: " << response;
            }
            else {
                string error = "Ошибка формата! Введите: Фамилия рост вес\n";
                send(clientSocket, error.c_str(), error.length(), 0);
            }
        }
    }
    catch (...) {
        std::cout << "Something went wrong!";
    }

    closesocket(clientSocket);
}

int main() {//добавить защиту от ddos(с уменьш в крит секции)
    try {
        SetConsoleOutputCP(1251);
        SetConsoleCP(1251);
        setlocale(LC_ALL, "Russian");

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
            return 1;
        }

        // Настройка адреса сервера
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;  // Принимаем подключения с любых адресов
        serverAddr.sin_port = htons(8888);         // Порт 8888

        // Привязка сокета к адресу
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cout << "Ошибка привязки сокета" << endl;
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        // Начинаем прослушивание
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            cout << "Ошибка прослушивания" << endl;
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        cout << "Сервер запущен на порту 8888" << endl;
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
            DWORD ipLength = INET_ADDRSTRLEN;
            WSAAddressToStringA((sockaddr*)&clientAddr, sizeof(clientAddr), NULL, clientIP, &ipLength);
            cout << "Новый клиент подключился: " << clientIP << endl;

            // Обрабатываем клиента в отдельном потоке
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handleClient, (LPVOID)clientSocket, 0, NULL);
        }

        closesocket(serverSocket);
        WSACleanup();
    }
    catch (...) {
        std::cout << "Something went wrong!";
    }

    return 0;
}
