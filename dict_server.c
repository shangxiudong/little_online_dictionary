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

void search_words(struct User_Psw users,int connfd,sqlite3* userdb)
{
	buffer buf;
	char buff[MAX];
	char buffe[MAX];
	char commad_line[128];
	FILE *fp;
	char *errmsg;
	time_t t;
	socklen_t len = sizeof(buf);
	if((fp = fopen("dict.txt","r")) == NULL)
	{
		perror("can not fopen");
		return;
	}
	while(1)
	{
		fseek(fp,0,SEEK_SET);
		bzero(commad_line,MAX);
		if(recv(connfd,&buf,len,0)<0)
		{
			perror("recv search_words");
		}
		//判断#号结束
		if(strncmp(buf.message,"#",1) == 0)
			return;
		//编辑数据库指令，添加至用户历史查询信息中
		strcpy(commad_line,"insert into ");
		strcat(commad_line,users.name);
		strcat(commad_line," values('");
		t = time(NULL);
		strcat(commad_line,asctime(localtime(&t)));
		strcat(commad_line,"','");
		strcat(commad_line,buf.message);
		strcat(commad_line,"');");
		if(sqlite3_exec(userdb,commad_line,NULL,NULL,&errmsg)!=0)
		{
			exit(0);
		}
		//遍历字典查询
		while(1)
		{
			bzero(buff,MAX);
			if(fgets(buff,MAX,fp) != NULL)
			{
				if(strncmp(buff,buf.message,strlen(buf.message)) == 0)
				{
					bzero(buffe,MAX);
					sscanf(buff,"%s",buffe);
					if(strlen(buffe) == strlen(buf.message))
					{
						buf.type = YES;
						strcpy(buf.message,buff);
						if(send(connfd,&buf,sizeof(buf),0)<0)
						{
							perror("send_search_words");
							exit(0);
						}
						break;
					}
				}
			}
			else
			{
				buf.type = NO;
				send(connfd,&buf,sizeof(buf),0);
				break;
			}
		}
	}
	fclose(fp);
}
void search_record(int connfd,sqlite3* userdb)//查询记录
{
	
	struct buffer buf;
	char commad_line[MAX];
	socklen_t len = sizeof(buf);
	int nrow,ncolumn;
	char **result;
	char *errmsg;
	int i;
	struct historyMSG message;
	
	if(recv(connfd,&buf,len,0)<0)
	{
		perror("recv_search_record");
		exit(0);
	}
	strcpy(commad_line,"select * from ");
	strcat(commad_line,buf.message);
	strcat(commad_line,";");
	if(sqlite3_get_table(userdb,commad_line,&result,&nrow,&ncolumn,&errmsg)!=0)
	{
		message.type = NO;
		if(send(connfd,(void*)&message,sizeof(message),0)<0)
		{
			perror("send");
			exit(0);
		}	
	}
	else
	{
		for(i = 0;i<=nrow;i++)
		{
			message.type = YES;
			strcpy(message.Time,result[i*ncolumn]);
			strcpy(message.Word,result[i*ncolumn+1]);
			if(send(connfd,&message,sizeof(message),0)<0)
			{
				perror("send");
				exit(0);
			}
		}
		message.type = NO;
		if(send(connfd,&message,sizeof(message),0)<0)
		{
			perror("send");
			exit(0);
		}
	}
	return;
}

void change_psw(struct User_Psw users,int connfd,sqlite3* userdb)//修改密码
{
	char *errmsg;
	char command_line[MAX];
	if(recv(connfd,&users,sizeof(users),0)<0)
	{
		perror("recv_change_psw");
		exit(0);
	}
	strcpy(command_line,"update register set psw = '");
	strcat(command_line,users.psw);
	strcat(command_line,"' where name = '");
	strcat(command_line,users.name);
	strcat(command_line,"';");
	if(sqlite3_exec(userdb,command_line,NULL,NULL,&errmsg)!=0)
	{
		users.type =NO;
		strcpy(users.name,errmsg);
		if(send(connfd,&users,sizeof(users),0)<0)
		{
			perror("send_change_psw");
			exit(0);
		}
	}
	else
	{
		users.type = YES;
		if(send(connfd,&users,sizeof(users),0)<0)
		{
			perror("send_change_psw");
			exit(0);
		}
	}
}


//将接收到的用户名密码信息存储至结构体中
int save_uers(void *para, int f_num, char *f_value[], char *f_name[])
{
	struct User_Psw *p = (User_Psw*)para;
	p->type = YES;
	strcpy(p->name,f_value[0]);
	strcpy(p->psw,f_value[1]);	
	return 0;
}

int main(int argc,const char *argv[])
{
	if(argc < 4)
	{
		printf("too few argument");
		printf("Usage:%s <ip><port><database>",argv[0]);
		exit(0);
	}
	pid_t pid;
	//数据库变量
	sqlite3* userdb;
	char *errmsg;
	int listenfd,connfd;
	struct sockaddr_in serveraddr,clientaddr;
	socklen_t peerlen = sizeof(serveraddr);
	struct User_Psw users;
	struct buffer buf;
	
	//创建服务器socket
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		perror("socket");
		exit(0);
	}
	//初始化服务器结构体
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	
	//绑定服务器文件描述符
	if(bind(listenfd,(struct sockaddr *)&serveraddr,peerlen)<0)
	{
		perror("bind");
		exit(0);
	}
	
	//设定服务器倾听模式
	if(listen(listenfd,5)<0)
	{
		perror("listen");
		exit(0);
	}
	//打开数据库
	if(sqlite3_open(argv[3],&userdb) != 0)
	{
		perror("sqlite3_open");
		exit(0);
	}
	//持续接收新客户端请求
	while(1)
	{
		if((connfd = accept(listenfd,(struct sockaddr *)&clientaddr,&peerlen))<0)
		{
			perror("accept");
			exit(0);
		}
		pid = fork();
		if(pid < 0)
		{
			perror("can not fork");
			exit(0);
		}
		if(pid > 0)
		{
			close(connfd);
		}
		if(pid == 0)
		{
			struct User_Psw dbuser;
			char login_name[MAX];
			char register_name[MAX];
			char commad_string[MAX];
			socklen_t len = sizeof(users);
			//循环登录界面
			while(1)
			{
				//清空缓存
				bzero(&users,sizeof(users));
				bzero(&dbuser,sizeof(dbuser));
				if(recv(connfd,&users,len,0)<0)
				{
					perror("recv");
					exit(0);
				}
				if(users.type == LOGIN)//登入账号
				{
					//判断数据初始化
					strcpy(login_name,"select * from register where name = '");
					strcat(login_name,users.name);
					strcat(login_name,"';");
					bzero(&dbuser,sizeof(dbuser));
					bzero(&buf,sizeof(buf));
					if(sqlite3_exec(userdb,login_name,save_uers,(void*)&dbuser,&errmsg)!=0)
					{//其他情况
						buf.type = NO;
						strcpy(buf.message,errmsg);
						send(connfd,&buf,sizeof(buf),0);
					}
					else if(dbuser.type != YES)//判断用户名
					{
						buf.type = NO;
						strcpy(buf.message,"没有该用户\n");
						send(connfd,&buf,sizeof(buf),0);		
					}
					else if(strcmp(users.psw,dbuser.psw)==0)//判断密码
					{						
						buf.type = YES;
						strcpy(buf.message,"登入成功\n");
						send(connfd,&buf,sizeof(buf),0);		
						break;//唯一出口
					}
					else//密码错误
					{
						buf.type = NO;
						strcpy(buf.message,"密码输入错误\n");
						send(connfd,&buf,sizeof(buf),0);
					}
					
				}
				else if(users.type == REGISTER)//注册账号
				{
					//判断是否有该账户
					strcpy(register_name, "select * from register where name = '");
					strcat(register_name,users.name);
					strcat(register_name,"';");
					bzero(&dbuser,sizeof(dbuser));
					dbuser.type = NO;
					if(sqlite3_exec(userdb,register_name,save_uers,(void*)&dbuser,&errmsg)!=0)
					{
						buf.type = NO;
						strcpy(buf.message,errmsg);
						send(connfd,&buf,sizeof(buf),0);
					}
					//判断用户名已存在
					else if(dbuser.type == YES)
					{
						buf.type = NO;
						strcpy(buf.message,"该用户已存在\n");
						send(connfd,&buf,sizeof(buf),0);
					}
					else
					{
						buf.type = YES;
						send(connfd,&buf,sizeof(buf),0);
						//等待接收密码
						if(recv(connfd,&users,len,0) <0 )
						{
							perror("recv");
						}
						//向register数据表新增该用户账户信息
						strcpy(commad_string,"insert into register values('");
						strcat(commad_string,users.name);
						strcat(commad_string,"','");
						strcat(commad_string,users.psw);
						strcat(commad_string,"');");
						if(sqlite3_exec(userdb,commad_string,NULL,NULL,&errmsg)!= 0 )
						{
							printf("%s\n",errmsg);
							exit(0);
						}
						//新增该用户table
						strcpy(commad_string,"create table ");
						strcat(commad_string,users.name);
						strcat(commad_string,"(time text,word text);");
						if(sqlite3_exec(userdb,commad_string,NULL,NULL,&errmsg)!= 0 )
						{
							printf("%s\n",errmsg);
							exit(0);
						}
						//唤醒客户端,注册成功
						buf.type = YES;
						strcpy(buf.message,"注册成功，请登录\n");
						send(connfd,&buf,sizeof(buf),0);
					}
				}
				else
				{
					sqlite3_close(userdb);
					close(connfd);
					close(listenfd);
					exit(0);
				}
					
			}
			//进入主菜单选项
			char C;
			int flag = 0;
			while(1)
			{
				bzero(&buf,sizeof(buf));
				if(recv(connfd,&C,sizeof(char),0)<0)
				{
					perror("recv 进入主菜单选项");
					exit(0);
				}
				switch(C)
				{
					case '1'://查单词
						search_words(users,connfd,userdb);
						break;
					case '2'://查用户记录
						search_record(connfd,userdb);
						break;
					case '3'://修改密码
						change_psw(users,connfd,userdb);
						break;
					case '4'://退出
						flag = 1;
						break;
					default:
						break;
				}
				if(flag) break;
			}
			sqlite3_close(userdb);
			close(connfd);
			close(listenfd);
			exit(0);
		}
	}
	 close(listenfd);
	 return 0;
}