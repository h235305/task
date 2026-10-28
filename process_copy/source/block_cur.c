#include<copy.h>

int block_cur(const char * srcfile,int pronum)
{
	//1.打开源文件
	int srcfd;
	srcfd = open(srcfile,O_RDONLY);

	if(srcfd == -1)
	{
		perror("open");
		return -1;
	}

	//2.计算源文件大小
	off_t filesize = lseek(srcfd,0,SEEK_END);

	//3.根据文件大小是否能被整除分情况
	if(filesize % pronum == 0)
	{
		return filesize / pronum;
	}
	else
	{
		return filesize / pronum + 1;
	}
}

