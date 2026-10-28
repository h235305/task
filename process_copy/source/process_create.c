#include<copy.h>

int process_create(const char* srcfile ,const char * destfile ,int pronum,int blocksize)
{
	pid_t pid;

	for(int i = 0;i < pronum;i++)
	{
		pid = fork();

		if(pid < 0)
		{
			perror("fork");
			process_wait(i);
			return -1;
		}
		else if(pid == 0)
		{
			int offset = i * blocksize; 

			char offset_str[10];
			char blocksize_str[10];
			bzero(offset_str,10);
			bzero(blocksize_str,10);
				
			//类型转换	
			snprintf(offset_str,sizeof(offset_str),"%d",offset);
			snprintf(blocksize_str,sizeof(blocksize_str),"%d",blocksize);

			execl("./Mod/copy","copy",srcfile,destfile,offset_str,blocksize_str,NULL);

			perror("execl");
			_exit(127);
		}
	}

	return 0;
}

