#ifndef __HEADER__
#define __HEADER__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define MAXNAME 20
#define MAXLINE 4096
#define MAXSOCK 1024

typedef struct user {
	char		name[MAXNAME];
	char		ip[16];
	int			port;
	int			fd;
} USR;

typedef struct message {
	int 		code;
	char		buf[MAXLINE];
	USR			user;
} MSG;

USR				client[FD_SETSIZE];
extern int		num_user, maxi;
time_t			ct;
struct 	tm		tm;

int				tcp_connect(char* port);
void			join_client(MSG msg, int sock, int i);
void 			exit_client(MSG msg, int sock, int i);
void 			receive_msg(MSG msg, USR usr, int sock, int i);
void			broadcast(MSG msg, int fd);
void			notice(MSG msg);
void			whisper(MSG msg, int tcp_fd);
void			user_list(MSG msg, int tcp_fd);
void			change_name(MSG msg, int sock, int i);
int				check_name(char* name);
MSG				make_chat(MSG msg);
void			menu(char host[], int port);
void    		error_handling(char *msg, int line);

#endif	/* __HEADER__ */