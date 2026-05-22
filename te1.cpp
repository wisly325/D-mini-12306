#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888

// 定义车次结构
typedef struct {
    int id;
    char code[10];
    char from[20];
    char to[20];
    char time[20];
    int price;
    int remaining;
} Train;

// 模拟车次数据（中文用 UTF-8 编码）
Train trains[] = {
    {1, "G82", "\xe6\xad\xa6\xe6\xb1\x89", "\xe5\x8c\x97\xe4\xba\xac", "08:30-14:45", 520, 50},
    {2, "G66", "\xe6\xad\xa6\xe6\xb1\x89", "\xe5\x8c\x97\xe4\xba\xac", "10:15-16:30", 520, 30},
    {3, "G70", "\xe6\xad\xa6\xe6\xb1\x89", "\xe5\x8c\x97\xe4\xba\xac", "14:00-20:15", 520, 20}
};
int train_count = 3;

// 生成 JSON
void get_trains_json(char* buffer) {
    char temp[512];
    strcpy(buffer, "[");
    for (int i = 0; i < train_count; i++) {
        sprintf(temp, "{\"id\":%d,\"code\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"time\":\"%s\",\"price\":%d,\"remaining\":%d}",
            trains[i].id, trains[i].code, trains[i].from, trains[i].to,
            trains[i].time, trains[i].price, trains[i].remaining);
        strcat(buffer, temp);
        if (i < train_count - 1) strcat(buffer, ",");
    }
    strcat(buffer, "]");
}

// 处理购票（消息也改成 UTF-8）
void buy_ticket(char* response, int train_id) {
    for (int i = 0; i < train_count; i++) {
        if (trains[i].id == train_id && trains[i].remaining > 0) {
            trains[i].remaining--;
            sprintf(response, "{\"success\":1,\"message\":\"\xe8\xb4\xad\xe7\xa5\xa8\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81%s %s\xe2\x86\x92%s ￥%d\"}",
                trains[i].code, trains[i].from, trains[i].to, trains[i].price);
            return;
        }
    }
    sprintf(response, "{\"success\":0,\"message\":\"\xe4\xbd\x99\xe7\xa5\xa8\xe4\xb8\x8d\xe8\xb6\xb3\"}");
}

// 读取文件内容
char* read_file(const char* filename, int* size) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) return NULL;

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* content = (char*)malloc(*size + 1);
    fread(content, 1, *size, fp);
    content[*size] = '\0';
    fclose(fp);

    return content;
}

// 处理客户端请求
DWORD WINAPI handle_client(LPVOID client_socket) {
    SOCKET sock = *(SOCKET*)client_socket;
    free(client_socket);

    char buffer[4096] = { 0 };
    char method[16] = { 0 };
    char path[256] = { 0 };

    recv(sock, buffer, sizeof(buffer) - 1, 0);
    sscanf(buffer, "%s %s", method, path);

    printf("收到请求: %s %s\n", method, path);

    char response[8192];
    int response_len;

    if (strcmp(path, "/api/trains") == 0 && strcmp(method, "GET") == 0) {
        char json[2048];
        get_trains_json(json);
        response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            (int)strlen(json), json);
        send(sock, response, response_len, 0);
    }
    else if (strncmp(path, "/api/buy?id=", 12) == 0 && strcmp(method, "GET") == 0) {
        int train_id = atoi(path + 12);
        char json[512];
        buy_ticket(json, train_id);
        response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            (int)strlen(json), json);
        send(sock, response, response_len, 0);
    }
    else {
        int file_size;
        char* html_content = read_file("index.html", &file_size);

        if (html_content != NULL) {
            response_len = snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                file_size, html_content);
            send(sock, response, response_len, 0);
            free(html_content);
            printf("已返回 index.html (%d 字节)\n", file_size);
        }
        else {
            const char* error = "{\"error\":\"Not Found\"}";
            response_len = snprintf(response, sizeof(response),
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                (int)strlen(error), error);
            send(sock, response, response_len, 0);
            printf("找不到 index.html\n");
        }
    }

    closesocket(sock);
    return 0;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);

    printf("========================================\n");
    printf("   Mini-12306 服务器已启动\n");
    printf("   监听端口: %d\n", PORT);
    printf("   访问地址: http://localhost:%d\n", PORT);
    printf("========================================\n");
    printf("按回车键可停止服务器...\n\n");

    while (1) {
        SOCKET* client_sock = (SOCKET*)malloc(sizeof(SOCKET));
        *client_sock = accept(server_sock, NULL, NULL);
        if (client_sock != NULL) {
            CreateThread(NULL, 0, handle_client, client_sock, 0, NULL);
        }
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}