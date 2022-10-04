#include <solution.h>
#include <solution.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define BUFFER_SIZE 8192


int is_PID(char*);

void lsof(void)
{    
    DIR *proc;
    
    proc = opendir("/proc/");
    if(proc == NULL)
    {
        report_error("/proc/", errno);
    }
    struct dirent *process;
    struct dirent *files;
    char path_fd[256 + sizeof("/proc/") + sizeof("/fd/")];
    char symlink[BUFFER_SIZE];
    while ((process = readdir(proc))) 
    {
        if (is_PID(process->d_name) == 0) 
        {
            continue;
        }	
        pid_t pid;
        sscanf(process->d_name, "%d", &pid);
        snprintf(path_fd, sizeof(path_fd), "/proc/%s/fd/", process->d_name);
        DIR *proc_fd = opendir(path_fd);
        if (proc_fd == NULL) 
        {
            continue;
        }
        errno = 0;
        while (1)
        {
            files = readdir(proc_fd);
            if (files == NULL && errno == 0)
            {
                break;
            } 
            if (errno != 0)
            {
                report_error(path_fd, errno);
                errno = 0;
            }
            snprintf(symlink, sizeof(symlink), "/proc/%s/fd/%s", process->d_name, files->d_name);
            char fd_path[BUFFER_SIZE];
            ssize_t nbytes = readlink(symlink, fd_path, BUFFER_SIZE);
            if (nbytes == -1) 
            {
                continue;
            }     
            report_file(fd_path);
        }
        if (closedir(proc_fd) == -1) {
            report_error("/proc/", errno);
        }  
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
