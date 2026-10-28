#include<copy.h>

int process_wait(int pronum)
{
	pid_t pid;
	int status;
	
	//用于检查回收过程是否有错误
	int failed = 0;

	
	for(int i=0;i<pronum;i++)
	{
		pid = waitpid(-1,&status,0);

		if(pid == -1)
		{
			perror("waitpid");
			return -1;
		}

		if(WIFEXITED(status))
		{
			int code = WEXITSTATUS(status);
			if(code != 0)
			{
				printf("子进程 ：%d ，退出码： %d\n",pid,code);
				failed = 1;
			}
		}
		else if(WIFSIGNALED(status))
		{
			printf("子进程：%d ，终止信号：%d\n",pid,WTERMSIG(status));
			failed = 1;
		}
		else
		{
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}
