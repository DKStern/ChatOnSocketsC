#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#ifndef PROTO
#define PROTO

#define LENGTH_NAME 31
#define LENGTH_MSG 101
#define LENGTH_SEND 201

#endif // PROTO
#ifndef LIST
#define LIST

typedef struct ClientNode {
    int data;
    struct ClientNode* prev;
    struct ClientNode* link;
    char ip[16];
    char name[31];
} ClientList;

ClientList *newNode(int sockfd, char* ip) {
    ClientList *np = (ClientList *)malloc( sizeof(ClientList) );
    np->data = sockfd;
    np->prev = NULL;
    np->link = NULL;
    strncpy(np->ip, ip, 16);
    strncpy(np->name, "NULL", 5);
    return np;
}

#endif // LIST

// Глобальные переменные
int server_sockfd = 0, client_sockfd = 0;
ClientList *root, *now;

bool KEY[256];
void GetKEY()
{
    int i = 0;
    while(i < 256)
    {
    if(GetAsyncKeyState(i)) KEY[i] = 1; else KEY[i] = 0;
    i++;
    }
}

void catch_ctrl_c_and_exit(int sig) {
    ClientList *tmp;
    while (root != NULL) {
        printf("\nСокет закрыт: %d\n", root->data);
        close(root->data); // Закрываем сокет
        tmp = root;
        root = root->link;
        free(tmp);
    }
    printf("Завершение...\n");
    exit(EXIT_SUCCESS);
}

void send_to_all_clients(ClientList *np, char tmp_buffer[]) {
    ClientList *tmp = root->link;
    while (tmp != NULL) {
        if (np->data != tmp->data) {
            printf("Отправлено сокету %d: \"%s\" \n", tmp->data, tmp_buffer);
            send(tmp->data, tmp_buffer, LENGTH_SEND, 0);
        }
        tmp = tmp->link;
    }
}

void esc_handler() {
    GetKEY()
    while (1) {
        if ( Key[27] ) {
            catch_ctrl_c_and_exit();
        }
    }    
}

void client_handler(void *p_client) {
    int leave_flag = 0;
    char nickname[LENGTH_NAME] = {};
    char recv_buffer[LENGTH_MSG] = {};
    char send_buffer[LENGTH_SEND] = {};
    ClientList *np = (ClientList *)p_client;

    // Проверка имён
    if (recv(np->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME-1) {
        printf("%s не ввёл имя!\n", np->ip);
        leave_flag = 1;
    } else {
        strncpy(np->name, nickname, LENGTH_NAME);
        printf("%s(%s)(%d) присоединился к чату!\n", np->name, np->ip, np->data);
        sprintf(send_buffer, "%s присоединился к чату!", np->name);
        send_to_all_clients(np, send_buffer);
    }

    // Общение
    while (1) {
        if (leave_flag) {
            break;
        }
        int receive = recv(np->data, recv_buffer, LENGTH_MSG, 0);
        if (receive > 0) {
            if (strlen(recv_buffer) == 0) {
                continue;
            }
            sprintf(send_buffer, "%s：%s", np->name, recv_buffer);
        } else if (receive == 0 || strcmp(recv_buffer, "exit") == 0) {
            printf("%s(%s)(%d) покинул чат!\n", np->name, np->ip, np->data);
            sprintf(send_buffer, "%s(%s) покинул чат!", np->name, np->ip);
            leave_flag = 1;
        } else {
            printf("Критическая ошибка: -1\n");
            leave_flag = 1;
        }
        send_to_all_clients(np, send_buffer);
    }

    // Удаление 
    close(np->data);
    if (np == now) {
        now = np->prev;
        now->link = NULL;
    } else {
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
    free(np);
}

int main()
{
    signal(SIGINT, catch_ctrl_c_and_exit);

    // Создание сокета
    server_sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sockfd == -1) {
        printf("Ошибка создания сокета!");
        exit(EXIT_FAILURE);
    }

    // Информация о сокете
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(8888);

    // Связывание и прослушка
    bind(server_sockfd, (struct sockaddr *)&server_info, s_addrlen);
    listen(server_sockfd, 5);

    getsockname(server_sockfd, (struct sockaddr*) &server_info, (socklen_t*) &s_addrlen);
    printf("Сервер запущен: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));

    // Создание списка клиентов
    root = newNode(server_sockfd, inet_ntoa(server_info.sin_addr));
    now = root;

    pthread_t esc;
    if (pthread_create(&esc, NULL, (void *)client_handler, (void *)c) != 0) {
        perror("Ошибка создания потока для выхода!\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr*) &client_info, (socklen_t*) &c_addrlen);

        getpeername(client_sockfd, (struct sockaddr*) &client_info, (socklen_t*) &c_addrlen);
        printf("Клиент %s:%d присоединился!\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

        // Добавляем клиента в список
        ClientList *c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, (void *)client_handler, (void *)c) != 0) {
            perror("Ошибка создания потока!\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
