/* �����ҷ������˳��� */

#include <stdlib.h>
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
#include <signal.h>
#include "common.h"

/* �����ҳ�Ա��Ϣ */
typedef struct _member 
{
    /* ��Ա���� */
    char *name;

    /* ��Ա socket ������ */
    int sock;

    /* ��Ա���������� */
    int grid;

    /* ��һ����Ա */
    struct _member *next;

    /* ǰһ����Ա */
    struct _member *prev;

} Member;

/* ��������Ϣ */
typedef struct _group 
{
    /* ���������� */
    char *name;

    /* ��������������������� */
    int capa;

    /* ��ǰռ���ʣ������� */
    int occu;

    /* ��¼�����������г�Ա��Ϣ������ */
    struct _member *mems;

} Group;

/* ���������ҵ���Ϣ�� */
Group *group;
int ngroups;

/* ͨ�������������ҵ������� ID */
int findgroup(char *name)
{
    int grid; /* ������ID */

	for (grid = 0; grid < ngroups; grid++)
	{
		if(strcmp(group[grid].name, name) == 0)
			return(grid);
	}
    return(-1);
}

/* ͨ���ҳ�Ա�����ҵ��ҳ�Ա����Ϣ */
Member *findmemberbyname(char *name)
{
    int grid; /* ������ ID */

    /* ����ÿ���� */
    for (grid=0; grid < ngroups; grid++) 
	{
        Member *memb;

        /* ������������г�Ա */
        for (memb = group[grid].mems; memb ; memb = memb->next)
		{
            if (strcmp(memb->name, name) == 0)
	        return(memb);
        }
    }
    return(NULL);
}

/* ͨ�� socket �������ҵ��ҳ�Ա����Ϣ */
Member *findmemberbysock(int sock)
{
    int grid; /* ������ID */

    /* �������е������� */
    for (grid=0; grid < ngroups; grid++) 
	{
		Member *memb;

		/* �������еĵ�ǰ�����ҳ�Ա */
		for (memb = group[grid].mems; memb; memb = memb->next)
		{
			if (memb->sock == sock)
			return(memb);
		}
    }
    return(NULL);
}

/* �˳�ǰ�������� */
void cleanup()
{
  char linkname[MAXNAMELEN];

  /* ȡ���ļ����� */
  sprintf(linkname, "%s/%s", getenv("HOME"), PORTLINK);
  unlink(linkname);
  exit(0);
}



/* ��ʼ������������ */
int initgroups(char *groupsfile)
{
	FILE *fp;
	char name[MAXNAMELEN];
	int capa;
	int grid;

	/* �򿪴洢��������Ϣ�������ļ� */
	fp = fopen(groupsfile, "r");
	if (!fp) 
	{
		fprintf(stderr, "error : unable to open file '%s'\n", groupsfile);
		return(0);
    }

	/* �������ļ��ж�ȡ�����ҵ����� */
	fscanf(fp, "%d", &ngroups);

	/* Ϊ���е������ҷ����ڴ�ռ� */
	group = (Group *) calloc(ngroups, sizeof(Group));
    if (!group) 
	{
		fprintf(stderr, "error : unable to calloc\n");
		return(0);
    }

	/* �������ļ���ȡ��������Ϣ */
	for (grid =0; grid < ngroups; grid++) 
	{
		/* ��ȡ�������������� */
		if (fscanf(fp, "%s %d", name, &capa) != 2)
		{
			fprintf(stderr, "error : no info on group %d\n", grid + 1);
			return(0);
		}

    /* ����Ϣ��� group �ṹ */
		group[grid].name = strdup(name);
		group[grid].capa = capa;
		group[grid].occu = 0;
		group[grid].mems = NULL;
    }
	return(1);
}

/* �����������ҵ���Ϣ�����ͻ��� */
int listgroups(int sock)
{
	int      grid;
	char     pktbufr[MAXPKTLEN];
	char *   bufrptr;
	long     bufrlen;

	/* ÿһ����Ϣ���ַ������� NULL �ָ� */
	bufrptr = pktbufr;
	for (grid=0; grid < ngroups; grid++) 
	{
		/* ��ȡ���������� */
		sprintf(bufrptr, "%s", group[grid].name);
		bufrptr += strlen(bufrptr) + 1;

		/* ��ȡ���������� */
		sprintf(bufrptr, "%d", group[grid].capa);
		bufrptr += strlen(bufrptr) + 1;

		/* ��ȡ������ռ���� */
		sprintf(bufrptr, "%d", group[grid].occu);
		bufrptr += strlen(bufrptr) + 1;
    }
	bufrlen = bufrptr - pktbufr;

	/* ������Ϣ���ظ��ͻ��������� */
	sendpkt(sock, LIST_GROUPS, bufrlen, pktbufr);
	return(1);
}

/* ���������� */
int joingroup(int sock, char *gname, char *mname)
{
	int       grid;
	Member *  memb;

	/* ��������������������� ID */
	grid = findgroup(gname);
	if (grid == -1) 
	{
		char *errmsg = "no such group";
		sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg); /* ���;ܾ�������Ϣ */
		return(0);
    }

	/* ����Ƿ������ҳ�Ա�����ѱ�ռ�� */
	memb = findmemberbyname(mname);

	/* ��������ҳ�Ա���Ѵ��ڣ��򷵻ش�����Ϣ */
	if (memb) 
	{
		char *errmsg = "member name already exists";
		sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg); /* ���;ܾ�������Ϣ */
		return(0);
    }

	/* ����������Ƿ����� */
	if (group[grid].capa == group[grid].occu) 
	{
		char *errmsg = "room is full";
		sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg); /* ���;ܾ�������Ϣ */
		return(0);
	}

	/* Ϊ�������³�Ա�����ڴ�ռ����洢��Ա��Ϣ */
	memb = (Member *) calloc(1, sizeof(Member));
	if (!memb) 
	{
		fprintf(stderr, "error : unable to calloc\n");
		cleanup();
    }
	memb->name = strdup(mname);
	memb->sock = sock;
	memb->grid = grid;
	memb->prev = NULL;
	memb->next = group[grid].mems;
	if (group[grid].mems) 
	{
		group[grid].mems->prev = memb;
	}
	group[grid].mems = memb;
	printf("admin: '%s' joined '%s'\n", mname, gname);

	/* ���������ҵ��������� */
	group[grid].occu++;

	sendpkt(sock, JOIN_ACCEPTED, 0, NULL); /* ���ͽ��ܳ�Ա��Ϣ */
	return(1);
}

/* �뿪������ */
int leavegroup(int sock)
{
	Member *memb;

	/* �õ������ҳ�Ա��Ϣ */
	memb = findmemberbysock(sock);
	if (!memb) 
		return(0);

	/* ����������Ϣ�ṹ��ɾ�� memb ��Ա */
	if (memb->next) 
		memb->next->prev = memb->prev; /* �������ҳ�Ա�����β�� */

	/* remove from ... */
	if (group[memb->grid].mems == memb) /* �������ҳ�Ա�����ͷ�� */
		group[memb->grid].mems = memb->next;

	else 
		memb->prev->next = memb->next; /* �������ҳ�Ա������в� */
	
	printf("admin: '%s' left '%s'\n",
		memb->name, group[memb->grid].name);

	/* ���������ҵ�ռ���� */
	group[memb->grid].occu--;

	/* �ͷ��ڴ� */
	free(memb->name);
	free(memb);
	return(1);
}

/* �ѳ�Ա����Ϣ���͸����������ҳ�Ա */
int relaymsg(int sock, char *text)
{
	Member *memb;
	Member *sender;
	char pktbufr[MAXPKTLEN];
	char *bufrptr;
	long bufrlen;

	/* ���� socket ��������ø������ҳ�Ա����Ϣ */
	sender = findmemberbysock(sock);
	if (!sender)
	{
		fprintf(stderr, "strange: no member at %d\n", sock);
		return(0);
	}

	/* �ѷ����ߵ�������ӵ���Ϣ�ı�ǰ�� */
	bufrptr = pktbufr;
	strcpy(bufrptr,sender->name);
	bufrptr += strlen(bufrptr) + 1;
	strcpy(bufrptr, text);
	bufrptr += strlen(bufrptr) + 1;
	bufrlen = bufrptr - pktbufr;

	/* �㲥����Ϣ���ó�Ա���������ҵ�������Ա */
	for (memb = group[sender->grid].mems; memb; memb = memb->next)
	{
		/* ���������� */
		if (memb->sock == sock) 
			continue;
		sendpkt(memb->sock, USER_TEXT, bufrlen, pktbufr); /* ��������������Ա
														  ������Ϣ��TCP��ȫ˫���ģ� */
	}
	printf("%s: %s", sender->name, text);
	return(1);
}
/* ���������� */
int main(int argc, char *argv[])
{
	int    servsock;   /* �����ҷ������˼��� socket ������ */
	int    maxsd;	     /* ���ӵĿͻ��� socket �����������ֵ */
	fd_set livesdset, tempset; /* �ͻ��� sockets �������� */


	/* �û�����Ϸ��Լ�� */
	if (argc != 2) 
		{
			fprintf(stderr, "usage : %s <groups-file>\n", argv[0]);
			exit(1);
		}

	/* ���� initgroups ��������ʼ����������Ϣ */
	if (!initgroups(argv[1]))
		exit(1);

	/* �����źŴ����� */
	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);

	/* ׼���������� */
	servsock = startserver(); /* ������ "chatlinker.c" �ļ��У�
							��Ҫ��ɴ����������׽��֣��󶨶˿ںţ�
							�����ð��׽���Ϊ����״̬ */
	if (servsock == -1)
		exit(1);

	/* ��ʼ�� maxsd */
	maxsd = servsock;

	/* ��ʼ���������� */
	FD_ZERO(&livesdset); /* ���� livesdset �����еı���λ*/
	FD_ZERO(&tempset);  /* ���� tempset �����еı���λ */
	FD_SET(servsock, &livesdset); /* �򿪷����������׽��ֵ��׽���
								  ������ servsock ��Ӧ��fd_set ����λ */

	/* ���ܲ��������Կͻ��˵����� */
	while (1) 
		{
			int sock;    /* ѭ������ */

			/* �ر�ע�� tempset ��Ϊ select ����ʱ��һ�� "ֵ-���" ������
			select ��������ʱ��tempset �д򿪵ı���λֻ�Ƕ������� socket
			����������������ÿ��ѭ����Ҫ�������Ϊ������Ҫ�ں˲��Զ���������
			�� socket ���������� livesdset */
			tempset = livesdset; 

		
			/* ���� select �����ȴ��������׽����ϵİ�������
			�µ��׽��ֵ��������� */
			select(maxsd + 1, &tempset, NULL, NULL, NULL);

			/* ѭ���������Կͻ��������� */
			for (sock=3; sock <= maxsd; sock++)
				{
					/* ����Ƿ��������� socket���������������ݰ����ڣ�ִ�н������� */
					if (sock == servsock)
						continue;

					/* �����Կͻ� socket ����Ϣ */
					if(FD_ISSET(sock, &tempset))
					{
						Packet *pkt;

						/* ����Ϣ */
						pkt = recvpkt(sock); /* ���� recvpkt ������"chatlinker.c" */

						if (!pkt)
							{
								/* �ͻ����Ͽ������� */
								char *clientname;  /* host name of the client */

								/* ʹ�� gethostbyaddr��getpeername �����õ� client �������� */
								socklen_t len;
								struct sockaddr_in addr;
								len = sizeof(addr);
								if (getpeername(sock, (struct sockaddr*) &addr, &len) == 0) 
									{
										struct sockaddr_in *s = (struct sockaddr_in *) &addr;
										struct hostent *he;
										he = gethostbyaddr(&s->sin_addr, sizeof(struct in_addr), AF_INET);
										clientname = he->h_name;
									}
								else
									printf("Cannot get peer name");

								printf("admin: disconnect from '%s' at '%d'\n",
									clientname, sock);

								/* ��������ɾ���ó�Ա */
								leavegroup(sock);

								/* �ر��׽��� */
								close(sock);

								/* ����׽����������� livesdset �еı���λ */
								FD_CLR(sock, &livesdset);

							} 
						else 
							{
								char *gname, *mname;

								/* ������Ϣ���Ͳ�ȡ�ж� */
								switch (pkt->type) 
								{
									case LIST_GROUPS :
										listgroups(sock);
										break;
									case JOIN_GROUP :
										gname = pkt->text;
										mname = gname + strlen(gname) + 1;
										joingroup(sock, gname, mname);
										break;
									case LEAVE_GROUP :
										leavegroup(sock);
										break;
									case USER_TEXT :
										relaymsg(sock, pkt->text);
										break;
								}

								/* �ͷŰ��ṹ */
								freepkt(pkt);
							}
					}
				}

			struct sockaddr_in remoteaddr; /* �ͻ�����ַ�ṹ */
			socklen_t addrlen;

			/* �������µĿͻ����������������� */
			if(FD_ISSET(servsock, &tempset))
			{
				int  csd; /* �����ӵ� socket ������ */

				/* ����һ���µ��������� */
				addrlen = sizeof remoteaddr;
				csd = accept(servsock, (struct sockaddr *) &remoteaddr, &addrlen);

				/* ������ӳɹ� */
				if (csd != -1) 
					{
						char *clientname;

						/* ʹ�� gethostbyaddr �����õ� client �������� */
						struct hostent *h;
						h = gethostbyaddr((char *)&remoteaddr.sin_addr.s_addr,
							sizeof(struct in_addr), AF_INET);

						if (h != (struct hostent *) 0) 
							clientname = h->h_name;
						else
							printf("gethostbyaddr failed\n");

						/* ��ʾ�ͻ������������Ͷ�Ӧ�� socket ������ */
						printf("admin: connect from '%s' at '%d'\n",
							clientname, csd);

						/* �������ӵ��׽��������� csd ����livesdset */
						FD_SET(csd, &livesdset);

						/* ���� maxsd ��¼���������׽��������� */
						if (csd > maxsd)
							maxsd = csd;
					}
				else 
					{
						perror("accept");
						exit(0);
					}
			}
		}
}


