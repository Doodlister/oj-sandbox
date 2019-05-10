#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>


void setProcessLimit(long timelimit, long memory_limit) {
	struct rlimit rl;
	/* set the time_limit (second)*/
	rl.rlim_cur = timelimit / 1000;
	rl.rlim_max = rl.rlim_cur + 1/1000;
	setrlimit(RLIMIT_CPU, &rl);

	/* set the memory_limit (b)*/
	rl.rlim_cur = memory_limit * 1024;
	rl.rlim_max = rl.rlim_cur + 1024;
	setrlimit(RLIMIT_DATA, &rl);
}
int run(char *args[], long timelimit, long memory_limit, char *in, char *out) {
	int pid = fork();
	if (pid < 0) {//子进程创建失败
		printf("error in fork chilid process \n");

	}
	else if (pid == 0) {//子进程业务逻辑
		int newstdin = open(in, O_RDWR | O_CREAT, 0644);
		int newstdout = open(out, O_RDWR | O_CREAT, 0644);
		if (newstdout != -1 && newstdin != -1) {
			if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) != -1) {
				printf("====== ok =====\n");
			}
			dup2(newstdout, fileno(stdout));
			dup2(newstdin, fileno(stdin));
			//1 秒 1Byte
			setProcessLimit(timelimit, memory_limit);
			printf("text prm output :%s \n", args[0]);
			if (execvp(args[0], args) == -1) {
				printf("execvp is error!\n");
			}
			close(newstdin);
			close(newstdout);
			exit(0);
		}
		else {
			printf("====== error =====\n");
		}

	}
	else {
		struct rusage ru;
		int status, time_used = 0, memory_used = 0;
		printf("the child pid is %d \n", pid);
		while (1) {
			if (wait4(pid, &status, WUNTRACED, &ru) == -1)
				printf("wait4 [WSTOPPED] failure");
			if (WIFEXITED(status)) { //未超时.. 计算时间、内存消耗
				time_used = ru.ru_utime.tv_sec * 1000
					+ ru.ru_utime.tv_usec / 1000
					+ ru.ru_stime.tv_sec * 1000
					+ ru.ru_stime.tv_usec / 1000;
				memory_used = ru.ru_maxrss;
				printf("child process is right!\n");
				printf("timeused: %d ms | memoryused : %d b\n", time_used, memory_used);
				return 1;
			}
			else if (WSTOPSIG(status) != SIGTRAP) {
				ptrace(PTRACE_KILL, pid, NULL, NULL);
				waitpid(pid, NULL, 0);
				time_used = ru.ru_utime.tv_sec * 1000
					+ ru.ru_utime.tv_usec / 1000
					+ ru.ru_stime.tv_sec * 1000
					+ ru.ru_stime.tv_usec / 1000;
				memory_used = ru.ru_maxrss;
				switch (WSTOPSIG(status)) {
				case SIGSEGV:
					if (memory_used > memory_limit)
						printf("Memory Limit Exceed\n");
					else
						printf("Runtime Error\n");
					break;
				case SIGALRM:
				case SIGXCPU: //CPU超时
					printf("Time Limit Exceed\n");
					break;
				default:
					printf("Runtime Error Sign is :%d\n", status);
					break;
				}
				printf("child process is wrong!\n");
				printf("timeused: %d ms | memoryused : %d b\n", time_used, memory_used);
				return 0;
			}
			ptrace(PTRACE_CONT, pid, NULL, NULL);
		}
		return -1;
	}
}

int main(int argc, char** argv)
{	
	
	char *args[] = { "/root/projects/SandBox/bin/x64/Release/dd",NULL};
	run(args, 1, 100, "1.in", "1.out");
}