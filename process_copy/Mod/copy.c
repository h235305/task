#include<copy.h>

int main(int argc ,char **argv)
{
	//检查参数个数
	if(argc != 5)
	{
		printf("请检查参数数量\n");
		return -1;
	}

	const char * srcfile = argv[1];
	const char * destfile = argv[2];
	int offset = atoi(argv[3]);
	int blocksize = atoi(argv[4]);
	printf("子进程 : %d, offset : %d, blocksize : %d\n",getpid(),offset,blocksize);
	
	//参数合理性检验
	if(offset < 0 || blocksize <0)
	{
		printf("偏移/块参数错误\n");
		return -1;
	}

	//1.打开源文件 只读
	int srcfd,destfd;
	srcfd = open(srcfile,O_RDONLY);
	if(srcfd == -1)
	{
		perror("open srcfile");
		return -1;
	}

	//2.打开目标文件只写,不存在则创建
	destfd = open(destfile,O_WRONLY | O_CREAT,0664);
	if(destfd == -1)
	{
		perror("open destfile");
		close(srcfd);
		return -1;
	}

	//3.改变读写指针位置
	if(lseek(srcfd,offset,SEEK_SET) == -1 || lseek(destfd,offset,SEEK_SET) == -1)
	{
		perror("lseek");
		close(srcfd);
		close(destfd);
		return -1;
	}

	ssize_t len;
	char buffer[blocksize];
	bzero(buffer,sizeof(buffer));
	
	int remaining = blocksize;
	//4.循环读取和写入数据
	while(remaining > 0)
	{
	    len = read(srcfd,buffer,remaining);

	    if(len == -1)
	    {
			perror("read");
			close(srcfd);
			close(destfd);
			return -1;
		 }
	
		if(len == 0) break;
	
		ssize_t written = 0;
	
		while(written <len)
		{
			ssize_t n = write(destfd,buffer + written,len - written);
	
			if(n <= 0)
			{
				if(n == -1) perror("write");
	
				close(srcfd);
				close(destfd);
				return -1;
			}
			written += n;	
		}
		remaining -= len;
	}

	//6.读写完毕，关闭文件描述符
	close(srcfd);
	close(destfd);
	
	return 0;
}
