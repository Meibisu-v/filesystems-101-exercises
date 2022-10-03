#include <solution.h>
// opendir
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define BUFFER_SIZE 8192

// -----------
#include <stdio.h>

int is_PID(char*);

void ps(void)
{
	DIR *proc;
	FILE *fd;
	proc = opendir("/proc/");
	if(proc == NULL)
	{
		report_error("/proc/", errno);
	}
    struct dirent *process;
	char path_environ[256 + sizeof("/proc") + sizeof("/environ") + 2]; // /proc + /environ+ //
	char path_exe[256 + sizeof("/proc") + sizeof("/exe") + 2]; // /proc + /exe + //
	char path_cmdline[256 + sizeof("/proc") + sizeof("/cmdline") + 2]; // /proc + /cmdline + //
	while ((process = readdir(proc))) {
		if (is_PID(process->d_name) == 0) {
			continue;
		}	
		pid_t pid;
		sscanf(process->d_name, "%d", &pid);
		snprintf(path_environ, sizeof(path_environ), "/proc/%s/environ", process->d_name);
		fd = fopen(path_environ, "r"); 
		if (!fd) {
			report_error(path_environ, errno);
			continue;
        }    

		char **environ_lines = NULL;
		int num_righe = 0;
		ssize_t nread;
		size_t len = 0;
		char *line = NULL;
		while(1) { 
			line = NULL;
			len = 0;
			nread = getdelim(&line, &len, '\0', fd);
			if (nread < 0) {
				break;
			}
			printf("OUT %s, len = %ld\n", line, len); 
			num_righe++;
			environ_lines = (char**)realloc(environ_lines, sizeof(char*)*(num_righe+1));
			environ_lines[num_righe-1] = strdup(line);
		}
		environ_lines[num_righe] = NULL;
		fclose(fd);
////
		snprintf(path_cmdline, sizeof(path_cmdline), "/proc/%s/cmdline", process->d_name);		
		fd = fopen(path_cmdline, "r"); 
		if (!fd) {
			report_error(path_cmdline, errno);
			continue;
        }    
		char **lines = NULL;
		num_righe = 0;
		
        while (1) {     
			line = NULL;
			len = 0;
			nread =  getdelim(&line, &len, '\0', fd);
			if (nread < 0) {
				break;
			}
			num_righe++;
			lines = (char**)realloc(lines, sizeof(char*)*(num_righe + 1));
			lines[num_righe-1] = strdup(line);
			line = NULL;
		}
		lines[num_righe] = NULL;
		fclose(fd);

		snprintf(path_exe, sizeof(path_exe), "/proc/%s/exe", process->d_name);		
        char exe_addr[BUFFER_SIZE];
        ssize_t nbytes = readlink(path_exe, exe_addr, BUFFER_SIZE);       
        if (nbytes == -1) {
               perror("readlink");
               exit(EXIT_FAILURE);
        }
        exe_addr[nbytes] = '\0';
        report_process(pid, exe_addr, lines, environ_lines);
		free(lines);
		free(line);
		free(environ_lines);
	}
}

int is_PID(char *str) {
	for (int i = 0; str[i] != '\0'; ++i) {
		if (isdigit(str[i]) == 0) {
			return 0;
		}
	}
	return 1;
} 
