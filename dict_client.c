#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <time.h>
#define MAX 128
#define LOGIN 0
#define REGISTER 1
#define YES 0
#define NO 1

typedef struct User_Psw{
	int type;
	char name[18];
	char psw[18];
}User_Psw;

typedef struct buffer{
	int type;
	char message[MAX];
}buffer;

typedef struct historyMSG{
	int type;
	char Time[30];
	char Word[30];
}historyMSG;
//查询单词函数
void search_words(int serverfd)
{
	struct buffer buf;
	int len = sizeof(buf);
	while(1)//循环搜索
	{
		bzero(buf.message,MAX);
		printf("请输入要查询的单词(#结束):\n");
		fgets(buf.message,MAX,stdin);
		*(buf.message + strlen(buf.message) -1) = '\0';
		send(serverfd,&buf,sizeof(buf),0);//发送至服务器
		if(strncmp(buf.message,"#",1) == 0) break;//判断#结束
		recv(serverfd,&buf,len,0);//等待接收服务器数据
		if(buf.type == NO)//如果没有这个单词
		{
			printf("没有该单词\n");
		}
		else//如果有单词
		{
			printf("%s\n",buf.message);
		}
	}
}
//查询历史记录
void search_history(int serverfd)
{
	struct buffer buf;
	struct historyMSG message;
	int len = sizeof(buf);
	printf("请输入要查询的用户名：");
	fgets(buf.message,MAX,stdin);
	*(buf.message + strlen(buf.message) -1) = '\0';
	send(serverfd,&buf,len,0);
	len = sizeof(message);
	while(1)
	{
		bzero(&message,sizeof(message));
		recv(serverfd,&message,len,0);
		if(message.type == NO)
		{
			printf("没有更多关于该用户的记录信息\n");
			break;
		}
		else
		{
			printf("%s\t%s\n",message.Time,message.Word);
		}
	}
	return;
}
//修改密码
void change_psw(int serverfd,struct User_Psw *users)
{
	int len;
	int i;
	char buf1[MAX];
	for(i=0;i<5;i++)
	{
		if(strcmp(users->psw,getpass("请输入原密码:"))==0)
		{
			strcpy(buf1,getpass("请输入新密码:"));
			if(strcmp(buf1,getpass("请确认密码:"))==0)
			{
				strcpy(users->psw,buf1);
				send(serverfd,users,sizeof(*users),0);
				recv(serverfd,users,len,0);
				if(users->type == YES)
				{
					printf("密码修改成功\n");
					return;
				}
				else
				{printf("密码未修改成功\n");}
			}
			else
			{printf("密码不相同\n");}
		}
		else
		{printf("密码错误\n");}
	}
		printf("修改密码超过五次，即将退出修改\n");
//	return;
}
//main函数
int main(int argc,const char*argv[])
{
	if(argc<3)
	{
		printf("too few argument");
		return 0;
	}
	int flag = 1;
	int serverfd;
	int i;//接收客户端指令
	struct sockaddr_in serveraddr;
	socklen_t peerlen = sizeof(serveraddr);
	struct User_Psw users;
	struct buffer buf;
	socklen_t len = sizeof(buf);
	
	if((serverfd = socket(AF_INET,SOCK_STREAM,0)) <0)
	{
		perror("socket");
		exit(0);
	}
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	
	if(connect(serverfd,(struct sockaddr*)&serveraddr,peerlen)<0)
	{
		perror("connect");
		exit(0);
	}
	//连接成功
	printf("connect success");
	//登入
	while(1)
	{
		bzero(&users,sizeof(users));
		bzero(&buf,sizeof(buf));
		printf("\n*****************************\n0.登入\n1.注册\n2.退出\n*****************************\n");
		scanf("%d",&users.type);
		getchar();
		//如果用户既没有输入1也没有输入2
		if(users.type != LOGIN &&users.type != REGISTER &&users.type != 2)
		{
			printf("请输入'0'、'1'或'2'\n");
		}
		//用户登录
		else if(users.type == LOGIN)
		{
			printf("请输入用户名:");
			fgets(users.name,MAX,stdin);
			*(users.name + strlen(users.name) -1) = '\0';
			strcpy(users.psw,getpass("请输入密码:"));
			send(serverfd,&users,sizeof(users),0);
			recv(serverfd,&buf,len,0);
			if(buf.type == YES)
			{
				printf("%s",buf.message);
				break;//登录界面唯一出口
			}
			else
			{
				printf("%s",buf.message);
			}
		}
		//用户注册
		else if(users.type == REGISTER)
		{
			printf("请输入用户名:");
			fgets(users.name,MAX,stdin);
			*(users.name + strlen(users.name) -1) = '\0';
			send(serverfd,&users,sizeof(users),0);
			recv(serverfd,&buf,len,0);
			if(buf.type == YES)
			{
				strcpy(users.psw,getpass("请输入密码:"));
				while(1)
				{
					if(strcmp(users.psw,getpass("请确认密码:")) == 0)
					{
						send(serverfd,&users,sizeof(users),0);
						recv(serverfd,&buf,len,0);
						if(buf.type == YES);
						printf("%s\n",buf.message);
						break;
					}
					else
						printf("输入错误请重新输入\n");
				}
			}
			else
				printf("该用户已存在，请重新输入\n");
		}
		else
		{
			send(serverfd,&users,sizeof(users),0);
			flag = 0;
			break;
		}
	}
	//进入主界面
	char C; //用于选择
	while(flag)
	{
		bzero(&buf,sizeof(buf));
		printf("\n*****************************\n1.查询单词\n2.查看用户记录\n3.修改密码\n4.退出\n*****************************\n");
		C = getc(stdin);
		getchar();
		switch(C)
		{
			case '1':
				send(serverfd,&C,sizeof(C),0);
				search_words(serverfd);
				break;
			case '2':
				send(serverfd,&C,sizeof(C),0);
				search_history(serverfd);
				break;
			case '3':
				send(serverfd,&C,sizeof(C),0);
				change_psw(serverfd,&users);
				break;
			case '4':
				send(serverfd,&C,sizeof(C),0);
				flag=0;
				break;
			default:
				printf("请输入数字1-4\n");
				break;
		}
	}
	close(serverfd);
	return 0;
	
}