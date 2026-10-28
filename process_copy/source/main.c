#include<copy.h>

int main(int argc,char ** argv)
{
	//检查参数个数并转换
	int pronum,blocksize;
		
	if(argc <3 || argc >4)
	{
		printf("参数数量error");
		return -1;
	}
	else if(argc == 3)
	{
		pronum = 1;
	}
	else
	{
		pronum = atoi(argv[3]);
	}
	
	if (strcmp(argv[1], argv[2]) == 0)
	{
    		printf("源文件和目标文件不能相同\n");
    		return -1;
	}

	//1.调用 check_pram()
	if(check_pram(argv[1],pronum) == -1) return -1;

	//2.调用 block_cur()
	if((blocksize = block_cur(argv[1],pronum)) == -1) return -1;

	//3.初始化目标文件，避免残留旧数据
	int destfd = open(argv[2],O_WRONLY | O_CREAT | O_TRUNC,0664);

	if(destfd == -1)
	{
		perror("open destfile");
		return -1;
	}
	close(destfd);

	//检查块值
	if (blocksize == 0) return 0;

	//4.创建子进程
	if(process_create(argv[1],argv[2],pronum,blocksize) == -1) return -1;

	//5.回收子进程
	if(process_wait(pronum) == -1) return -1;

	return 0;
}
