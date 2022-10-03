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
        size_t environ_size = 0;
        size_t ar_size = 1;
        ssize_t nread;
        size_t len = 0;
        char *line = NULL;
        environ_lines = (char**)realloc(environ_lines, sizeof(char*)*(ar_size));
        while(1) { 
            nread = getdelim(&line, &len, '\0', fd);
            if (nread < 0) {
                break;
            }
            environ_size++;
            if (environ_size >= ar_size) 
            {
                ar_size = ar_size * 2;
                environ_lines = (char**)realloc(environ_lines, sizeof(char*)*(ar_size));
            }
            environ_lines[environ_size-1] = strdup(line);
        }
        if (environ_lines != NULL) 
        {
            environ_lines[environ_size] = NULL;
        }
        fclose(fd);
////
        snprintf(path_cmdline, sizeof(path_cmdline), "/proc/%s/cmdline", process->d_name);		
        fd = fopen(path_cmdline, "r"); 
        if (!fd) {
            report_error(path_cmdline, errno);
            continue;
        }    
        char **lines = NULL;
        size_t cmdline_size = 0;        
        while (1) {    
            nread =  getdelim(&line, &len, '\0', fd);
            if (nread < 0) {
                break;
            }
            cmdline_size++;
            lines = (char**)realloc(lines, sizeof(char*)*(cmdline_size + 1));
            lines[cmdline_size-1] = strdup(line);
        }
        lines[cmdline_size] = NULL;
        fclose(fd);

        snprintf(path_exe, sizeof(path_exe), "/proc/%s/exe", process->d_name);		
        char exe_addr[BUFFER_SIZE];
        ssize_t nbytes = readlink(path_exe, exe_addr, BUFFER_SIZE);       
        if (nbytes == -1) {
            report_error(path_exe, errno);
            continue;
        }
        exe_addr[nbytes] = '\0';
        report_process(pid, exe_addr, lines, environ_lines);
        for (char **x = lines; *x != NULL; ++x)
        {
            free(*x);
        }		
        for (size_t i = 0; i < environ_size; ++i)
        {
            free(environ_lines[i]);
        }
        free(lines);
        free(line);
        free(environ_lines);
    }
    if (closedir(proc) == -1) {
        report_error("/proc/", errno);
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
