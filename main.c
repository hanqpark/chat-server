#include "server.h"

int num_user = 0;
int maxi = -1;

int main(int argc, char **argv) {
    int                 serv_sock, clnt_sock, sock, max_sock, nready, i;
	USR					usr;
	MSG        			msg;
    fd_set		       	reads, temps;
	ssize_t				nbyte;
    socklen_t           clnt_addr_sz;
    struct sockaddr_in  serv_addr, clnt_addr;

    /* bin 실행 시, PORT 입력했는지 확인 */
    if(argc != 2) {
        printf("Usage: %s <PORT> \n", argv[0]);
        exit(1);
    }

	serv_sock = tcp_connect(argv[1]);
	max_sock = serv_sock;
	FD_ZERO(&reads); FD_ZERO(&temps);
	FD_SET(0, &temps); FD_SET(serv_sock, &temps);
		
    while(1) {
        bzero(&(msg), sizeof(msg));
        reads = temps;
        nready = select(max_sock+1, &reads, NULL, NULL, NULL);
        if(nready == -1)
            error_handling("select() error \n", __LINE__);
        else {
			/*  Client의 접속 요청  */
            if (FD_ISSET(serv_sock, &reads)) {
                clnt_addr_sz = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
                if(clnt_sock == -1)
					error_handling("accept() error \n", __LINE__);

				// Client IP, PORT, 소켓번호 등록 (닉네임 초기화)
				for (i = 0; i < FD_SETSIZE; i++) {
					if (client[i].fd < 0) {
						client[i].fd = clnt_sock;
						client[i].port = clnt_addr.sin_port;
						bzero(&(client[i].name), sizeof(client[i].name));
						strcpy(client[i].ip, inet_ntoa(clnt_addr.sin_addr));
						break;
					}
				}

				// Connect 오류: Server Full
				if (i == FD_SETSIZE) {
					msg.code = 13;
					write(clnt_sock, &msg, sizeof(msg));
					continue;
				}

				// Socket 처리
    			FD_SET(clnt_sock, &temps);
                if(clnt_sock > max_sock)
					max_sock = clnt_sock;
				if(i > maxi)
					maxi = i;

				// Client에 Name 정보 전송 요청
				msg.code = 11;
				msg.user = client[i];
				write(clnt_sock, &msg, sizeof(msg));

                if(--nready <= 0)
                	continue;
            }

			/*  Server Terminal에서 메시지 입력  */
			if (FD_ISSET(0, &reads)) {
				notice(msg);

				if(--nready <= 0)
                	continue;
			}
				
        	/*  Client로부터 메시지 수신  */
        	for(i=0; i<=maxi; i++) {
        		if((sock = client[i].fd) < 0)
        			continue;
        		if(FD_ISSET(sock, &reads)) {
        			nbyte = read(sock, &msg, sizeof(msg));

					// TCP exit. FIN 수신
					if (nbyte == 0) {
						close(sock);
						FD_CLR(sock, &temps);
						exit_client(msg, sock, i);
						break;
					}

					// 메시지 수신
					else
						receive_msg(msg, usr, sock, i);

					if (--nready <= 0)
						break;
        		}
        	}
    	}
	}
    return 0;
}
