#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/wait.h>

int check_pram(const char* ,int );
int block_cur(const char * ,int );
int process_create(const char * ,const char * ,int ,int );
int process_wait(int);

