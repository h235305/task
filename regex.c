#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main(void)
{
    // 1. open + fstat + mmap
    int fd = open("url.txt", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (st.st_size <= 0)
    {
        fprintf(stderr, "文件为空或大小无效\n");
        close(fd);
        return 1;
    }

    size_t size = st.st_size;
    char* data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) { perror("mmap"); return 1; }

    // 2. 声明两个 regex_t，设计两条正则
    const char* pattern =
        "<a[[:space:]]+[^>]*href[[:space:]]*=[[:space:]]*"
        "(\"([^\"]*)\"|'([^']*)')[^>]*>([^<]*)</a[[:space:]]*>";

    const char* title_pattern =
        "[[:space:]]title[[:space:]]*=[[:space:]]*"
        "(\"([^\"]*)\"|'([^']*)')";

    // 3. regcomp 编译
    regex_t reg, treg;
    int ret;
    ret = regcomp(&reg, pattern, REG_EXTENDED | REG_ICASE);
    if (ret != 0)
    {
        char errbuf[128];

        regerror(ret, &reg, errbuf, sizeof(errbuf));
        fprintf(stderr, "reg error: %s\n", errbuf);

        munmap(data, size);
        return 1;
    }

    ret = regcomp(&treg, title_pattern, REG_EXTENDED | REG_ICASE);
    if (ret != 0)
    {
        char errbuf[128];

        regerror(ret, &treg, errbuf, sizeof(errbuf));
        fprintf(stderr, "treg error: %s\n", errbuf);

        regfree(&reg);
        munmap(data, size);
        return 1;
    }

    // 4. 声明 regmatch_t m[5], t[4]
    regmatch_t m[5], t[4];

    // 5. 设置 pos = 0
    regoff_t pos = 0;
    int count = 0;

    // 6. while 循环
    while (pos < (regoff_t)size)
    {
        //    设置 REG_STARTEND 搜索范围
        m[0] = (regmatch_t){ pos,(regoff_t)size };

        //    执行 regexec
        ret = regexec(&reg, data, 5, m, REG_STARTEND);
        if (ret != 0) break;

        //    选择 URL 捕获组
        regmatch_t url = m[m[2].rm_so != -1 ? 2 : 3];
        regmatch_t title = m[4];

        //    默认标题为正文
        t[0] = (regmatch_t){ m[0].rm_so,m[4].rm_so };
        if (regexec(&treg, data, 4, t, REG_STARTEND) == 0)
        {
            title = t[t[2].rm_so != -1 ? 2 : 3];
        }

        //    检查 title 属性
        //    清理空白
        regmatch_t* parts[] = { &url,&title };
        for (int i = 0; i < 2; i++)
        {
            regmatch_t* p = parts[i];
            while (p->rm_so < p->rm_eo && isspace((unsigned char)data[p->rm_so]))
            {
                p->rm_so++;
            }
            while (p->rm_so < p->rm_eo && isspace((unsigned char)data[p->rm_eo - 1]))
            {
                p->rm_eo--;
            }

        }
        //    输出有效新闻
        if (url.rm_so < url.rm_eo && title.rm_so < title.rm_eo)
        {
            printf("[%d] 地址：%.*s\n标题：%.*s\n\n", ++count,
                (int)(url.rm_eo - url.rm_so), data + url.rm_so,
                (int)(title.rm_eo - title.rm_so), data + title.rm_so);
        }
        //    更新 pos
        pos = m[0].rm_eo;
    }

    // 7. regfree + munmap
    if (ret != REG_NOMATCH && ret != 0)
    {
        fprintf(stderr, "regexec error\n");
    }

    printf("有效新闻数：%d\n", count);
    regfree(&reg);
    regfree(&treg);
    munmap(data, size);
    return ret == 0 || ret == REG_NOMATCH ? 0 : 1;
}