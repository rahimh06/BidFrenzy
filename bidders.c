#include "auction.h"
#include <stdarg.h>

void log_event(int pipe_fd, const char *fmt, ...)
{
    if(pipe_fd < 0)
    {
        return;
    }
    char buf[256];
    
    //format the string with arguments
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    
    buf[len++] = '\n';
    write(pipe_fd, buf, len); //spit it into the pipe
}

pid_t spawn_logger(int pipe_read_fd)
{
    pid_t pid = fork();
    if(pid == 0)
    {
        int log_fd = open("history.log", O_CREAT | O_WRONLY | O_APPEND, 0666);
        char buf[512];
        ssize_t n;
        
        //blocking read until pipe write ends are closed
        while((n = read(pipe_read_fd, buf, sizeof(buf))) > 0)
        {
            write(log_fd, buf, n);
        }
        
        close(log_fd);
        close(pipe_read_fd);
        _exit(0);
    }
    close(pipe_read_fd);
    g_shm->logger_pid = pid;
    return pid;
}
