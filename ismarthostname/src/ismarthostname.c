#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define dhcp_file "/tmp/dhcp.leases"
#define hostname_file "/etc/dhcp.leases"
#define time_refresh 600

struct user
{
	char hostname[256];
	char mac[32];
	char ip[32];
	int enable;
};

struct user all_user[256] = {0};

int readconf()
{
	char oneline[256] = {0};
	char mac[32] = {0};
	char ip[32] = {0};
	char hostname[256] = {0};
	unsigned char nip[4] = {0};
	FILE *hostname_fp = NULL;
	hostname_fp = fopen(hostname_file, "w+");
	if (!hostname_fp)
	{
		perror("fopen");
		return 1;
	}
	while (NULL != fgets(oneline, 256, hostname_fp))
	{
		sscanf(oneline, "%s %s %s", mac, ip, hostname);
		inet_pton(AF_INET, ip, &nip);
		memcpy(all_user[nip[3]].hostname, hostname, 256);
		memcpy(all_user[nip[3]].mac, mac, 32);
		memcpy(all_user[nip[3]].ip, ip, 32);
		all_user[nip[3]].enable = 1;
		printf("readconf: %s %s %s\n", mac, ip, hostname);
	}
	fclose(hostname_fp);
	return 0;
}

int printconf()
{
	FILE *hostname_fp = NULL;
	hostname_fp = fopen(hostname_file, "w+");
	if (!hostname_fp)
	{
		perror("fopen");
		return 1;
	}
	unsigned int i = 0;
	for (i = 2; i < 255; i++)
	{
		if (all_user[i].enable)
		{
			fprintf(hostname_fp, "%s %s %s\n", all_user[i].mac, all_user[i].ip, all_user[i].hostname);
			printf("printfconf: %s %s %s\n", all_user[i].mac, all_user[i].ip, all_user[i].hostname);
		}
	}
	fclose(hostname_fp);
	return 0;
}

int readdhcp()
{
	char oneline[256] = {0};
	char time[32] = {0};
	char mac[32] = {0};
	char ip[32] = {0};
	char hostname[256] = {0};
	unsigned char nip[4] = {0};
	FILE *dhcp_fp = NULL;
	dhcp_fp = fopen(dhcp_file, "r");
	if (!dhcp_fp)
	{
		perror("fopen");
		return -1;
	}
	while (NULL != fgets(oneline, 256, dhcp_fp))
	{
		sscanf(oneline, "%s %s %s %s", time, mac, ip, hostname);
		inet_pton(AF_INET, ip, &nip);
		memcpy(all_user[nip[3]].hostname, hostname, 256);
		memcpy(all_user[nip[3]].mac, mac, 32);
		memcpy(all_user[nip[3]].ip, ip, 32);
		all_user[nip[3]].enable = 1;
		printf("readdhcp %d: %s %s %s\n", nip[3], mac, ip, hostname);
	}

	fclose(dhcp_fp);

	return 0;
}

int arg_option(int argc, char *argv[])
{
	int opt;
	while((opt=getopt(argc,argv, "d")) != -1)
	{
		switch(opt)
		{
			case 'd':
			{
				daemon(0, 0);
				break;
			}
			default:
			{
				break;
			}

		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	arg_option(argc, argv);
	readconf();
	while (1)
	{
		readdhcp();
		printconf();
		sleep(time_refresh);
	}
	return 0;
}
