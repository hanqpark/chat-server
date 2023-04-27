#include "server.h"

int  tcp_connect(char* port) {
	int 				serv_sock, i;
	struct sockaddr_in  serv_addr;

	// socket 생성
    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
		error_handling("socket() error! \n", __LINE__);

    // 서버 주소 초기화
    bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(port));

    // socket에 주소 할당
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error! \n", __LINE__);

    // client 접속 요청 대기
	if(listen(serv_sock, MAXSOCK) == -1)
		error_handling("listen() error \n", __LINE__);

	for(i=0; i<FD_SETSIZE; i++)
		client[i].fd = -1;

	// Server측 Terminal에 메뉴 띄우기
	menu(inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));

	return serv_sock;
}

void join_client(MSG msg, int sock, int i) {
	ct = time(NULL);					// 현재 시간을 받아옴
	tm = *localtime(&ct);
	strcpy(client[i].name, msg.user.name);
	num_user++;

	write(1, "\033[0G", 4);				// 커서의 X좌표를 0으로 이동
	fprintf(stderr, "\033[1;33m");		// 글자색을 노란색으로 변경
	bzero(&(msg.buf), sizeof(msg.buf));
	sprintf(msg.buf, "[%02d:%02d:%02d] %s님이 입장하셨습니다. (현재 접속자 수: %d)\n", tm.tm_hour, tm.tm_min, tm.tm_sec, client[i].name, num_user);
	printf("%s", msg.buf);
	
	msg.code = 10;
	write(sock, &msg, sizeof(msg));

	msg.code = 21;
    broadcast(msg, sock);
}

void exit_client(MSG msg, int sock, int i) {
	char tmpname[MAXNAME];
	ct = time(NULL);					// 현재 시간을 받아옴
	tm = *localtime(&ct);

	bzero(tmpname, sizeof(tmpname));
	strcpy(tmpname, client[i].name);

	bzero(&(client[i].ip), sizeof(client[i].ip));
	bzero(&(client[i].port), sizeof(client[i].port));
	bzero(&(client[i].name), sizeof(client[i].name));
	client[i].fd = -1;

	if (strlen(tmpname)) {
		num_user--;
		write(1, "\033[0G", 4);				// 커서의 X좌표를 0으로 이동
		fprintf(stderr, "\033[1;33m");		// 글자색을 노란색으로 변경
		bzero(&(msg.buf), sizeof(msg.buf));
		sprintf(msg.buf, "[%02d:%02d:%02d] %s님이 퇴장하셨습니다. (현재 접속자 수: %d)\n", tm.tm_hour, tm.tm_min, tm.tm_sec, tmpname, num_user);
		printf("%s", msg.buf);
		msg.code = 21;
		write(sock, &msg, sizeof(msg));
		broadcast(msg, sock);
	}
}

void receive_msg(MSG msg, USR usr, int sock, int i) {
	/* Connect: Name 정보 수신 후 중복 확인 */
	if (msg.code == 11){ 
		if (check_name(msg.user.name))
			join_client(msg, sock, i);
		else {
			msg.code = 12;
			write(sock, &msg, sizeof(msg));
		}
	}

	/* Function: Chat to everyone */
	else if (msg.code == 20) {
		msg = make_chat(msg);
		broadcast(msg, sock);
	}

	/* Function: 1:1 direct message */
	else if (msg.code == 30)
		whisper(msg, sock);

	/* Function: list user */
	else if (msg.code == 40)
		user_list(msg, sock);

	/* Function: change user name */
	else if (msg.code == 50) 
		change_name(msg, sock, i);
}

// broadcast message except for myself
void broadcast(MSG msg, int fd) {
	int 		i;
	socklen_t	len;

	for(i=0; i<=maxi; i++) {
		if (client[i].fd < 0 || client[i].fd == fd)
			continue;
		else
			write(client[i].fd, &msg, sizeof(msg));
	}
}

// broadcast message to everyone
void notice(MSG msg) {
	int 	i;
	char	buffer[MAXLINE];

	bzero(&buffer, sizeof(buffer));
	read(0, buffer, sizeof(buffer));

	msg.code = 22;
	sprintf(msg.buf, "[서버 공지사항] %s", buffer);
	for (i=0; i<=maxi; i++) {
		if (client[i].fd < 0)
			continue;
		else
			write(client[i].fd, &msg, sizeof(msg));
	}

	write(1, "\033[0G", 4);				// 커서의 X좌표를 0으로 이동
	fprintf(stderr, "\033[1A"); 		// Y좌표를 현재 위치로부터 -1만큼 이동
	fprintf(stderr, "\033[1;35m");		// 글자색을 연보라색으로 변경
	fprintf(stderr, "%s", msg.buf);			
}

void whisper(MSG msg, int tcp_fd) {
	int 	i;
	char*	des_name = malloc(sizeof(char)*MAXNAME);
	char*	from_name = malloc(sizeof(char)*MAXNAME);
	char*	tmpstr = malloc(sizeof(char)*MAXLINE);

	ct = time(NULL);					// 현재 시간을 받아옴
	tm = *localtime(&ct);

	// cut string
	char* ptr = strtok(msg.buf, " ");	// "/dm"
	strcpy(des_name, strtok(NULL, " "));	// <NAME>
	strcpy(tmpstr, strtok(NULL, ""));	// content

	/*  33. DM 오류: 자기 자신에게 전송  */
	if (!strcmp(msg.user.name, des_name)) {
		msg.code = 33;
		sprintf(msg.buf, "자기 자신에게 DM을 보낼 수 없습니다! \n");
		write(tcp_fd, &msg, sizeof(msg));
	}

	else {
		// make DM
		bzero(&(msg.buf), sizeof(msg.buf));
		strcpy(msg.buf, tmpstr);

		// find destination.
		for (i = 0; i <= FD_SETSIZE; i++) {
			if (client[i].fd < 0)
				continue;
			else if (!strcmp(des_name, client[i].name)) {
				write(client[i].fd, &msg, sizeof(msg));
				break;
			}
		}
		
		// 사용자가 존재하지 않을 경우
		if (i > maxi) {
			msg.code = 32;
			sprintf(msg.buf, "존재하지 않는 사용자 입니다! \n");
			write(tcp_fd, &msg, sizeof(msg));
		}

		else {
			msg.code = 31;
			strcpy(msg.user.name, des_name);
			write(tcp_fd, &msg, sizeof(msg));
		}
	}
	free(des_name);
	free(from_name);
	free(tmpstr);
}

void user_list(MSG msg, int sock) {
	int 	i;
	char*	list_name = malloc(sizeof(char)*30);

	bzero(&(msg.buf), sizeof(msg.buf));
	strcat(msg.buf, "\n======== client list ========\n");
	sprintf(list_name, "현재 접속자 수: %d\n", num_user);
	strcat(msg.buf, list_name);
	for(i=0; i<=maxi; i++) {
		if(client[i].fd < 0)
			continue;
		else {
			if(client[i].fd == sock)
				sprintf(list_name, "[%s] me \n", client[i].name);
			else
				sprintf(list_name, "[%s] \n", client[i].name);
			strcat(msg.buf, list_name);
		}
	}
	strcat(msg.buf, "=============================\n\n");

	msg.code = 40;
	write(sock, &msg, sizeof(msg));

	free(list_name);
}

void change_name(MSG msg, int sock, int i) {
	char 	name[MAXNAME];
	int		j;

	// cut string
	char* ptr = strtok(msg.buf, " ");	// "/change"
	strcpy(name, strtok(NULL, " "));	// "<NAME>"
	for (j=0; j<MAXNAME; j++) {			// <NAME>에서 \n 제거
		if (name[j] == '\n') {
			name[j] = 0;
			break;
		}
	}

	if (!strcmp(msg.user.name, name)) {
		msg.code = 52;
		sprintf(msg.buf, "현재 닉네임과 일치합니다! 다른 닉네임을 입력해주세요. \n");
	}

	else if (check_name(name)) {
		msg.code = 50;
		strcpy(client[i].name, name);
		strcpy(msg.user.name, name);
		sprintf(msg.buf, "[%s]으로 닉네임이 변경되었습니다. \n", msg.user.name);
	}
	else {
		msg.code = 51;
		sprintf(msg.buf, "[%s]는 이미 존재하는 닉네임입니다! 다른 닉네임을 입력해주세요. \n", msg.user.name);
	}
	write(sock, &msg, sizeof(msg));
}

int  check_name(char* name) {
	for (int i=0; i<=maxi; i++) {
		if (!strcmp(client[i].name, name))
			return 0;
	}
	return 1;
}

MSG  make_chat(MSG msg) {
	char tmpstr[MAXLINE];

	bzero(tmpstr, sizeof(tmpstr));
	strcpy(tmpstr, msg.buf);
	bzero(&(msg.buf), sizeof(msg.buf));
	ct = time(NULL);					// 현재 시간을 받아옴
	tm = *localtime(&ct);
	sprintf(msg.buf, "[%02d:%02d:%02d] %s > %s", tm.tm_hour, tm.tm_min, tm.tm_sec, msg.user.name, tmpstr);

	return msg;
}

void menu(char* host, int port){
	system("clear");
	printf(" **** CHAT SERVER ****\n");
	printf(" Server IP\t: %s \n", host);
    printf(" Server PORT\t: %d \n\n", port);
    printf(" ****          Log         ****\n");
}

void error_handling(char *msg, int line) {
	fputs(msg, stderr);
    printf("Error occured at line %d \n", line);
	fputc('\n', stderr);
	exit(1);
}