/*************************************************************************
	> File Name: thread_pool.c
	> Author: suyelu 
	> Mail: suyelu@126.com
	> Created Time: Thu 09 Jul 2020 02:50:28 PM CST
 ************************************************************************/

#include "head.h"
extern int repollfd, bepollfd;
extern struct User *rteam, *bteam;
extern pthread_mutex_t bmutex;
extern pthread_mutex_t rmutex;

void send_all(struct ChatMsg *msg) 
{
    for (int i = 0; i < MAX; i++) {
        if (rteam[i].online == 1) {
            send(rteam[i].fd, (void*)msg, sizeof(struct ChatMsg), 0);
        }
        if (bteam[i].online == 1) {
            send(bteam[i].fd, (void*)msg, sizeof(struct ChatMsg), 0);
        }
    } 
}

void send_to(char* to, struct ChatMsg *msg, int fd)
{
    int flag = 0;
    for (int i = 0; i < MAX; i++) {
        if (rteam[i].online && !strcmp(to, rteam[i].name)) {
            send(rteam[i].fd, msg, sizeof(struct ChatMsg), 0);
            flag = 1;
            break;
        }
        if (bteam[i].online && !strcmp(to, bteam[i].name)) {
            send(bteam[i].fd, msg, sizeof(struct ChatMsg), 0);
            flag = 1;
            break;
        }
    }
    if (flag == 0) {  //没有找到私聊的对象
        memset(msg->msg, 0, sizeof(msg->msg));
        sprintf(msg->msg, "用户名 %s 不在线, 或用户名错误!", to);
        msg->type = CHAT_SYS;     //系统信息
        send(fd, msg, sizeof(struct ChatMsg), 0);
    }
}

void do_work(struct User *user)
{
    //收到一条信息，并打印。
    struct ChatMsg msg;
    struct ChatMsg r_msg;
    recv(user->fd, &msg, sizeof(msg), 0);
    if (msg.type & CHAT_WALL) {   //公聊信息
        printf("<%s>: %s\n", user->name, msg.msg);
        bzero(&r_msg, sizeof(r_msg));
        strcpy(r_msg.name, user->name);
        strcpy(r_msg.msg, msg.msg);
        send_all(&r_msg);       //广播
    } else if (msg.type & CHAT_MSG) {     //私聊
        char to[20] = {0};
        int i = 1;
        for (; i <= 21; i++) {
            if (msg.msg[i] == ' ') {
                break;
            }
        }
        if (msg.msg[i] != ' ' || msg.msg[0] != '@') {   //格式不对
            memset(&r_msg, 0, sizeof(r_msg));
            r_msg.type = CHAT_SYS;
            sprintf(r_msg.msg, "私聊格式错误!\n");
            send(user->fd, (void*)&r_msg, sizeof(r_msg), 0);
        } else {
            strncpy(to, msg.msg + 1, i - 1);
            bzero(&r_msg, sizeof(r_msg));
            strcpy(r_msg.name, user->name);
            strcpy(r_msg.msg, msg.msg + i + 1);
            r_msg.type = CHAT_MSG;
            send_to(to, &r_msg, user->fd);
        }
    } else if (msg.type & CHAT_FIN) {  //登出
        bzero(r_msg.msg, sizeof(r_msg.msg));
        r_msg.type = CHAT_SYS;
        sprintf(r_msg.msg, "%s 已经从聊天室中登出。\n", user->name);
        strcpy(r_msg.name, user->name);
        send_all(&msg);
        if (user->team)
            pthread_mutex_lock(&bmutex);
        else 
            pthread_mutex_lock(&rmutex);

        for (int i = 0; i < MAX; i++) {
            if (strcmp(rteam[i].name, user->name) == 0) {
                rteam[i].online = 0;
                break;
            }
            if (strcmp(bteam[i].name, user->name) == 0) {
                bteam[i].online = 0;
                break;
            }
        }
        //将该成员登出的信息通告所有人
        int epo = user->team ? bepollfd : repollfd; 
        del_event(epo, user->fd);
        if (user->team)
            pthread_mutex_unlock(&bmutex);
        else 
            pthread_mutex_unlock(&rmutex);
        printf(GREEN"Server Info"NONE" : %s 已经从聊天室中登出。\n", user->name);
        close(user->fd);
    } else if (msg.type & CHAT_FUNC) {   //返回在线人员列表
        struct ChatMsg list;
        bzero(&list, sizeof(list));
        list.type = CHAT_SYS;
        strcpy(list.name, "System");       //这条消息是系统发的所以命名为系统
        //制作名单
        for (int i = 0; i < MAX; i++) {
            if (rteam[i].online == 1) {
                strcat(list.msg, rteam[i].name);
                strcat(list.msg, "\n");
            }

            if (bteam[i].online == 1) {
                strcat(list.msg, bteam[i].name);
                strcat(list.msg, "\n");
            }
        }
        //制作名单完成
        send(user->fd, &list, sizeof(list), 0);    //发送
    }
    DBG("In do_work %s\n", user->name);
}

void task_queue_init(struct task_queue *taskQueue, int sum, int epollfd) {
    taskQueue->sum = sum;
    taskQueue->epollfd = epollfd;
    taskQueue->team = calloc(sum, sizeof(void *));
    taskQueue->head = taskQueue->tail = 0;
    pthread_mutex_init(&taskQueue->mutex, NULL);
    pthread_cond_init(&taskQueue->cond, NULL);
}

void task_queue_push(struct task_queue *taskQueue, struct User *user) {
    pthread_mutex_lock(&taskQueue->mutex);
    taskQueue->team[taskQueue->tail] = user;
    DBG(L_GREEN"Thread Pool"NONE" : Task push %s\n", user->name);
    if (++taskQueue->tail == taskQueue->sum) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue End\n");
        taskQueue->tail = 0;
    }
    pthread_cond_signal(&taskQueue->cond);
    pthread_mutex_unlock(&taskQueue->mutex);
}


struct User *task_queue_pop(struct task_queue *taskQueue) {
    pthread_mutex_lock(&taskQueue->mutex);
    while (taskQueue->tail == taskQueue->head) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue Empty, Waiting For Task\n");
        pthread_cond_wait(&taskQueue->cond, &taskQueue->mutex);
    }
    struct User *user = taskQueue->team[taskQueue->head];
    DBG(L_GREEN"Thread Pool"NONE" : Task Pop %s\n", user->name);
    if (++taskQueue->head == taskQueue->sum) {
        DBG(L_GREEN"Thread Pool"NONE" : Task Queue End\n");
        taskQueue->head = 0;
    }
    pthread_mutex_unlock(&taskQueue->mutex);
    return user;
}

void *thread_run(void *arg) {
    pthread_detach(pthread_self());
    struct task_queue *taskQueue = (struct task_queue *)arg;
    while (1) {
        struct User *user = task_queue_pop(taskQueue);
        do_work(user);
    }
}

