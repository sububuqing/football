/*************************************************************************
	> File Name: udp_epoll.c
	> Author: suyelu 
	> Mail: suyelu@126.com
	> Created Time: Thu 09 Jul 2020 04:40:38 PM CST
 ************************************************************************/

#include "head.h"
extern int port;
extern struct User *rteam;
extern struct User *bteam;

extern int repollfd, bepollfd;

int udp_connect(struct sockaddr_in *client) {
    int sockfd;
    if ((sockfd = socket_create_udp(port)) < 0) {
        perror("socket_create_udp");
        return -1;
    }
    if (connect(sockfd, (struct sockaddr *)client, sizeof(struct sockaddr)) < 0) {
        return -1;
    }
    return sockfd;
}

int check_online(struct LogRequest* request) {
    for (int i = 0; i < MAX; i++) {
        if (rteam[i].online == 1 && (strcmp(request->name, rteam[i].name) == 0)) {
            return 0;
        }
        if (bteam[i].online == 1 && (strcmp(request->name, bteam[i].name) == 0)) {
            return 0;
        }
    }
    return 1;
}

int udp_accept(int fd, struct User *user) {
    int new_fd, ret;
    struct sockaddr_in client;
    struct LogRequest request;
    struct LogResponse response;
    bzero(&request, sizeof(request));
    bzero(&response, sizeof(response));
    socklen_t len = sizeof(client);
    
    ret = recvfrom(fd, (void *)&request, sizeof(request), 0, (struct sockaddr *)&client, &len); 
    if (ret != sizeof(request)) {
        response.type = 1;
        strcpy(response.msg, "Login failed with Data errors!");
        sendto(fd, (void *)&response, sizeof(response), 0, (struct sockaddr *)&client, len);
        return -1;
    } 
    if (check_online(&request) == 0) {    //此处验证重复登录
        response.type = 1;
        strcpy(response.msg, "You are Already Login!");
        sendto(fd, (void *)&response, sizeof(response), 0, (struct sockaddr *)&client, len);
        return -1;
    }

    response.type = 0;
    strcpy(response.msg, "Login Success. Enjoy yourself!");
    sendto(fd, (void *)&response, sizeof(response), 0, (struct sockaddr *)&client, len);
    
    if (request.team) {
        DBG(GREEN"Info"NONE" : "BLUE"%s login on %s:%d  <%s>\n", request.name, inet_ntoa(client.sin_addr), ntohs(client.sin_port), request.msg);
    } else {
        DBG(GREEN"Info"NONE" : "RED"%s login on %s:%d   <%s>\n", request.name, inet_ntoa(client.sin_addr), ntohs(client.sin_port), request.msg);
    }
    strcpy(user->name, request.name);
    user->team = request.team;
    new_fd = udp_connect(&client);
    user->fd = new_fd;

    response.type = 0;


    return new_fd;
}


void add_event_ptr(int epollfd, int fd, int events, struct User *user)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = (void*)user;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void del_event(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

int find_sub(struct User *team) 
{
    for (int i = 0; i < MAX; i++) {
        if(!team[i].online)
            return i;
    }
    return -1;
}

void add_to_sub_reactor(struct User* user)
{

    struct User* team = (user->team ? bteam : rteam);
    int index = find_sub(team);
    if (index < 0){
        return;
    }
    /*
    if (user->team) {
        pthread_mutex_lock(&bmutex);
    } else {
        pthread_mutex_lock(&rmutex);
    }
    */
    team[index] = *user;
    team[index].online = 1;
    team[index].flag = 10;
    /*
    if (user->team) {
        pthread_mutex_unlock(&bmutex);
    } else {
        pthread_mutex_unlock(&rmutex);
    }
    */
    DBG(L_RED"sub = %d, name = %s\n"NONE, index, team[index].name);
    int events = EPOLLIN | EPOLLET;
    if (user->team) {
        add_event_ptr(bepollfd, user->fd, events, &team[index]);
    } else {
        add_event_ptr(repollfd, user->fd, events, &team[index]);
    }

}

