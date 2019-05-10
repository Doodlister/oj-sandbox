#include <stdio.h>
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

#include "JudgeService.h"
#include "jni.h"

/**
		*  0 Wait
		* 1 ：AC	Accepted	通过
		* 2 :WA	Wrong Answer	答案错误
		* 3 :TLE	Time Limit Exceed	超时
		* 4 :OLE	Output Limit Exceed	超过输出限制
		* 5 :MLE	Memory Limit Exceed	超内存
		* 6 :RE	Runtime Error	运行时错误
		* 7 :PE	Presentation Error	格式错误
		* 8 :CE	Compile Error	无法编译
*/


void setProcessLimit(long timelimit, long memory_limit) {
	struct rlimit rl;
	/* set the time_limit (second)*/
	rl.rlim_cur = timelimit / 1000;
	rl.rlim_max = rl.rlim_cur + 1 / 1000;
	setrlimit(RLIMIT_CPU, &rl);

	/* set the memory_limit (b)*/
	rl.rlim_cur = memory_limit * 1024;
	rl.rlim_max = rl.rlim_cur + 1024;
	setrlimit(RLIMIT_DATA, &rl);
}
typedef struct {
	int status;
	int time;
	int memory;
}Result;

Result* run(char *args[], long timelimit, long memory_limit, char *in, char *out) {
	Result* result = (Result*)malloc(sizeof(Result));
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

				result->memory = memory_used;
				result->time = time_used;
				result->status = 0;
				return result; //Accept
			}
			else if (WSTOPSIG(status) != SIGTRAP) {
				ptrace(PTRACE_KILL, pid, NULL, NULL);
				waitpid(pid, NULL, 0);
				time_used = ru.ru_utime.tv_sec * 1000
					+ ru.ru_utime.tv_usec / 1000
					+ ru.ru_stime.tv_sec * 1000
					+ ru.ru_stime.tv_usec / 1000;
				memory_used = ru.ru_maxrss;

				result->memory = memory_used;
				result->time = time_used;

				switch (WSTOPSIG(status)) {
				case SIGSEGV:
					if (memory_used > memory_limit) {
						printf("Memory Limit Exceed\n");
						result->status = 5;
						return result; //Memory Limit
					}
					else {
						printf("Runtime Error\n");
						result->status = 6;
						return result;//Runtime Error
					}
					break;
				case SIGALRM:
				case SIGXCPU: //CPU超时
					printf("Time Limit Exceed\n");
					result->status = 3;
					return result;//Time Limit Execcd
					break;
				default:
					printf("Runtime Error Sign is :%d\n", status);
					result->status = 6;
					return result;//Runtime Error
					break;
				}
				printf("child process is wrong!\n");
				printf("timeused: %d ms | memoryused : %d b\n", time_used, memory_used);
				result->status = 6;
				return result;//Runtime Error
			}
			ptrace(PTRACE_CONT, pid, NULL, NULL);
		}
		return -1;
	}
}




JNIEXPORT jstring JNICALL Java_JudgeService_sandbox(JNIEnv *env, jclass jc, jstring runPath, jstring inputPath, jint timeLimit, jint memoryLimit) {
	const char *runPuthBuff;
	const char *inputBuff;
	Result *result;
	//读入路径
	runPuthBuff = (*env)->GetStringUTFChars(env, runPath, NULL);
	char *args[] = { NULL,NULL };
	args[0] = runPuthBuff;
	//读入输入
	inputBuff = (*env)->GetStringUTFChars(env, inputPath, NULL);
	char *inputArg;
	inputArg = inputBuff;

	char *outputArg = (char*)malloc(sizeof(char) * 512);
	//生成输出目录
	outputArg = strcpy(outputArg, inputArg);
	outputArg = strcat(outputArg, ".out");

	result=run(args, timeLimit, memoryLimit, inputArg, outputArg);
	//释放环境
	(*env)->ReleaseStringUTFChars(env, runPath, runPuthBuff);
	(*env)->ReleaseStringUTFChars(env, inputPath, inputBuff);
	return (*env)->NewStringUTF(env, outputArg);
}

int main(int argc, char** argv)
{

	//char *args[] = { "/root/projects/SandBox/bin/x64/Release/dd",NULL };
	//run(args, 1, 100, "1.in", "1.out");
	//char *arg = args[0];
	//printf("%s", arg);
}