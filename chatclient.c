
/* �����ҿͻ��˳��� */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "common.h"

#define QUIT_STRING "/end"

/* ��ӡ���������� */
void showgroups(long lent, char *text) 
{
	char *tptr;

	tptr = text;
	printf("%15s %15s %15s\n", "group", "capacity", "occupancy");
	while (tptr < text + lent) 
	{
		char *name, *capa, *occu;

		name = tptr;
		tptr = name + strlen(name) + 1;
		capa = tptr;
		tptr = capa + strlen(capa) + 1;
		occu = tptr;
		tptr = occu + strlen(occu) + 1;

		printf("%15s %15s %15s\n", name, capa, occu);
	}
}

/* ���������� */
int joinagroup(int sock) {
	
	Packet * pkt;
	char bufr[MAXPKTLEN];
	char * bufrptr;
	int bufrlen;
	char * gname;
	char * mname;

	/* ������������Ϣ */
	sendpkt(sock, LIST_GROUPS, 0, NULL);

	/* ������������Ϣ�ظ� */
	pkt = recvpkt(sock);
	if (!pkt) 
	{
		fprintf(stderr, "error: server died\n");
		exit(1);
	}

	if (pkt->type != LIST_GROUPS) 
	{
		fprintf(stderr, "error: unexpected reply from server\n");
		exit(1);
	}

	/* ��ʾ������ */
	showgroups(pkt->lent, pkt->text);

	/* �ӱ�׼��������������� */
	printf("which group? ");
	fgets(bufr, MAXPKTLEN, stdin);
	bufr[strlen(bufr) - 1] = '\0';

	/* ��ʱ�����û����˳� */
	if (strcmp(bufr, "") == 0
			|| strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0)
	{
		close(sock);
		exit(0);
	}
	gname = strdup(bufr);

	/* �����Ա���� */
	printf("what nickname? ");
	fgets(bufr, MAXPKTLEN, stdin);
	bufr[strlen(bufr) - 1] = '\0';

	/* ��ʱ�����û����˳� */
	if (strcmp(bufr, "") == 0
			|| strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0) 
	{
		close(sock);
		exit(0);
	}
	mname = strdup(bufr);

	/* ���ͼ��������ҵ���Ϣ */
	bufrptr = bufr;
	strcpy(bufrptr, gname);
	bufrptr += strlen(bufrptr) + 1;
	strcpy(bufrptr, mname);
	bufrptr += strlen(bufrptr) + 1;
	bufrlen = bufrptr - bufr;
	sendpkt(sock, JOIN_GROUP, bufrlen, bufr);

	/* ��ȡ���Է������Ļظ� */
	pkt = recvpkt(sock);
	if (!pkt) 
	{
		fprintf(stderr, "error: server died\n");
		exit(1);
	}
	if (pkt->type != JOIN_ACCEPTED && pkt->type != JOIN_REJECTED) 
	{
		fprintf(stderr, "error: unexpected reply from server\n");
		exit(1);
	}

	/* ����ܾ���ʾ��ԭ�� */
	if (pkt->type == JOIN_REJECTED)
	{
		printf("admin: %s\n", pkt->text);
		free(gname);
		free(mname);
		return (0);
	}
	else /* �ɹ����� */
	{
		printf("admin: joined '%s' as '%s'\n", gname, mname);
		free(gname);
		free(mname);
		return (1);
	}
}

/* ��������� */
main(int argc, char *argv[]) 
{
	int sock;

	/* �û�����Ϸ��Լ�� */
	if (argc != 1) 
	{
		fprintf(stderr, "usage : %s\n", argv[0]);
		exit(1);
	}

	/* ����������� */
	sock = hooktoserver();
	if (sock == -1)
		exit(1);

	fflush(stdout); /* �����׼��������� */

	
	/* ��ʼ���������� */
	fd_set clientfds, tempfds;
	FD_ZERO(&clientfds);
	FD_ZERO(&tempfds);
	FD_SET(sock, &clientfds); /* ���÷������׽����� clientfds �еı���λ */
	FD_SET(0, &clientfds); /* ���ñ�׼������ clientfds �еı���λ */

	/* ѭ�� */
	while (1) 
	{
		/* ���������� */
		if (!joinagroup(sock))
			continue;

		/* ��������״̬ */
		while (1) 
		{
			/* ���� select ����ͬʱ�����̺ͷ�������Ϣ */
			tempfds = clientfds;

			if (select(FD_SETSIZE, &tempfds, NULL, NULL, NULL) == -1) 
			{
				perror("select");
				exit(4);
			}

			/* ���������� tempfds �б���λ���ļ���������������Ƿ����׽�����������
			����ǣ���ζ������������Ϣ��������ļ��������� 0������ζ�������û�
			���̵�����Ҫ���͸������� */

			/* ���������������Ϣ */
			if (FD_ISSET(sock,&tempfds)) 
			{
				Packet *pkt;
				pkt = recvpkt(sock);
				if (!pkt) 
				{
					/* ������崻� */
					fprintf(stderr, "error: server died\n");
					exit(1);
				}

				/* ��ʾ��Ϣ�ı� */
				if (pkt->type != USER_TEXT) 
				{
					fprintf(stderr, "error: unexpected reply from server\n");
					exit(1);
				}

				printf("%s: %s", pkt->text, pkt->text + strlen(pkt->text) + 1);
				freepkt(pkt);
			}

			/* ����������� */
			if (FD_ISSET(0,&tempfds)) 
			{
				char bufr[MAXPKTLEN];

				fgets(bufr, MAXPKTLEN, stdin);
				if (strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0) 
				{
					/* �˳������� */
					sendpkt(sock, LEAVE_GROUP, 0, NULL);
					break;
				}

				/* ������Ϣ�ı��������� */
				sendpkt(sock, USER_TEXT, strlen(bufr) + 1, bufr);
			}

		}

	}
}

