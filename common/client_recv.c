/*************************************************************************
	> File Name: client_recv.c
	> Author: 
	> Mail: 
	> Created Time: Fri 10 Jul 2020 04:41:09 PM CST
 ************************************************************************/

#include "head.h"
extern int sockfd;

void* do_recv(void* arg) 
{
    while (1) {
        struct ChatMsg msg;
        bzero(&msg, sizeof(msg));
        int ret = recv(sockfd, (void*)&msg, sizeof(msg), 0);
        if (msg.type & CHAT_WALL) {   //公聊
            printf(""BLUE"<%s>"NONE" : %s\n", msg.name, msg.msg);
        } else if (msg.type & CHAT_MSG) {   //私聊
            printf(""RED"<%s>"NONE" : %s\n", msg.name, msg.msg);
        } else if (msg.type & CHAT_SYS) {    //系统消息
            printf(YELLOW"Server Info"NONE" : %s\n", msg.msg);
        } else if (msg.type & CHAT_FIN) {   //服务器主动登出
            printf(L_RED"Server Info"NONE"Server Down!\n");
            exit(1);
        }
    }

}
