#include<copy.h>

int check_pram( const char * srcfile ,int pronum)
{
	//1.判断参数数量
	//if(argc < 3 || argc >4)
	//{
	//	printf("参数数量error");
	//	return 1;
	//}
	
	//直接在main函数里面判断，还可以避免非法访问

	//2.srcfile源文件是否存在
	if((access(srcfile,F_OK))!=0)
	{
		printf("源文件error");
		return -1;
	}


	//3.判断进程数量是否合理
	if(pronum < 1 || pronum >=100)
	{
		printf("进程数量(应在1-99)error");
		return -1;
	}

	return 0;
}
