#include <string>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <vector>

std::string GetCurrentPath()
{
    char buf[PATH_MAX] = {0};
    int len = readlink("/proc/self/exe", buf, PATH_MAX);
    if(len < 0 || len >= PATH_MAX)
    {
        printf("readlink len=%d, error=%s\n", len, strerror(errno));
        return "";
    }
 
    buf[len] = '\0';
    for(int i = len; i >= 0; --i)
    {
        if (buf[i] == '/')
        {
            buf[i+1] = '\0';
            break;
        }
    }
 
    return buf;
}
 
#define MAX_WRITE_INFO 100
#define MAX_INFO_LEN 1024
static void LogInfo(const char * fmt, ...)
{
	char szInfo[MAX_INFO_LEN] = {0};
	
	va_list arglist;
    va_start(arglist, fmt);
    vsnprintf(szInfo, sizeof(szInfo)-1, fmt, arglist);
    va_end(arglist);
	
    std::vector<std::string> strMsgs;
 
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
 
        struct tm tmtime;
        gmtime_r(&tv.tv_sec, &tmtime);
 
        char szTime[64] = {0};
        snprintf(szTime, sizeof(szTime)-1, "%4d%02d%02d %02d:%02d:%02d.%06ld", tmtime.tm_year + 1900, tmtime.tm_mon + 1, 
					tmtime.tm_mday, tmtime.tm_hour, tmtime.tm_min, tmtime.tm_sec, tv.tv_usec);
 
		int tid = syscall(SYS_gettid);
        char szMsg[MAX_INFO_LEN] = {0};
        snprintf(szMsg, sizeof(szMsg)-1, "%s [%d] - %s\n", szTime, tid, szInfo);
		
		printf("%s", szMsg);
        strMsgs.emplace_back(szMsg);
        FILE * fp = fopen("daemon.log", "r");
        if(fp)
        {
            char szMsg[MAX_INFO_LEN] = {0};
            for(size_t i = 0; i < MAX_WRITE_INFO-1 && !feof(fp); ++i)
            {
                fgets(szMsg, sizeof(szMsg), fp);
                strMsgs.emplace_back(szMsg);
            }
 
            fclose(fp);
        }
    }
 
    {
        FILE * fp = fopen("daemon.log", "w");
        if(!fp)
        {
            return;
        }
 
        for(size_t i = 0; i < strMsgs.size(); ++i)
        {
            fwrite(strMsgs[i].c_str(), strMsgs[i].size(), sizeof(char), fp);
        }
 
        fclose(fp);
    }
}
 
static bool SingleInstance(const char * instance)
{
    const char * name = strrchr(instance, '/');
    if(!name)
    {
        name = instance;
    }
 
    char path[PATH_MAX] = {0};
    snprintf(path, sizeof(path), "%s.%s.lck", GetCurrentPath().c_str(), name);
 
    int fd = open(path, O_CREAT|O_RDWR, 0666);
    if(fd < 0)
    {
        printf("open=%s, error=%s", path, strerror(errno));
        return false;
    }
 
    int lock_result = lockf(fd, F_TLOCK, 0);
    if(lock_result < 0)
    {
        return false;
    }
 
    return true;
}
 
static void PrintUsage(char* name)
{
     printf("\n ----- \n\n"
            "Usage:\n"
            "   	%s program_name \n\n"
            "Where:\n"
            "   	%s - Name of this Daemon loader.\n"
            "   	program_name - Name (including path) of the program you want to load as daemon.\n\n"
            "Example:\n"
            "   	%s ./atprcmgr - Launch program 'atprcmgr' in current directory as daemon. \n\n\n\n",
            name, name, name);
}
 
static pid_t g_child_pid = 0;
 
static void HandleSignal(int signo)
{
    if(signo == SIGTERM)
    {
        if(g_child_pid != 0)
        {
            kill(g_child_pid, SIGTERM);
        }
	    
        exit(0);
    }
}
 
int main(int argc, char* argv[])
{
    printf("\nDaemon loader\n- Launch specified program as daemon.\n");
    if(argc < 2)
    {
        printf("* Missing parameter : daemon program name not specified!\n");
        PrintUsage(argv[0]);
        return 0;
    }
 
    switch(fork())
    {
        case 0:  break;
        case -1:
                printf("\n! Create daemon programm %s error=%s. \n", argv[1], strerror(errno));
                return -1;
        default: exit(0);
    }
 
    if(setsid() < 0)
    {
        printf("\n! Create daemon programm %s error=%s. \n", argv[1], strerror(errno));
        return -1;
    }
 
    if(!SingleInstance(argv[1]))
    {
        printf("\n! The program %s was runing already!!!\n\n\n", argv[1]);
        return 0;
    }
 
    signal(SIGTERM, HandleSignal);
    printf("- Loading %s as daemon, please wait ......\n\n\n", argv[1]);
	
    while(true)
    {
        pid_t pid = fork();
        if(pid == 0)
        {
            signal(SIGCHLD, SIG_IGN);
            LogInfo("Programm %s running!!!", argv[1]);
            if(execv(argv[1], argv+1) != 0)
            {
                LogInfo("Can't Excute The programm %s", argv[1]);
                return -1;
            }
        }
        else if(pid > 0)
        {
			LogInfo("Start daemon programm %s", argv[1]);
			
            g_child_pid = pid;
            int status = 0;
            wait(&status);
            g_child_pid = 0;
			
			LogInfo("status=%d, Restart daemon programm %s after 10s.", status, argv[1]);
            sleep(10);
        }
        else
        {
            LogInfo("Create daemon programm %s error=%s.", argv[1], strerror(errno));
            break;
        }
    }
 
    return 0;
}
