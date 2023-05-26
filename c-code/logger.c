#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>


#ifndef SIZE
#define SIZE (256)
#else
#error SIZE is already declared
#endif //SIZE

#define LOG_FILE_PATH     "/etc/notx/syslogmask.txt"
#define MAX_FILENAME_LEN  15 /* includes extension */
#define bzero(b,len)      (memset((b), '\0', (len)), (void) 0)

FILE* fp=NULL;


void openlogger()
{
    openlog("NOTX", LOG_CONS | LOG_NOWAIT | LOG_PERROR | LOG_PID, LOG_LOCAL0);
}

void closelogger()
{
    closelog();
}

void parse(char* p_buf)
{
    char ext[5] = {0};
    char* t1=p_buf;
    char* t2;
    char* t3;

    assert(p_buf!=NULL);

    t3=(char*)calloc(MAX_FILENAME_LEN + 1, sizeof(char));
    if (t3)
    {
        int len;
        strcat(t1,"\0");

        /* Omit trailing directory descriptors */
        if ( (t2=strrchr(t1, '/')) )
        {
            t1=t2+1;
        }

        /* Store file extension */
        if ( (t2=strrchr(t1, '.')) )
        {
            strncpy((char*)&ext,t2,strlen(t2));
        }

        /* Update file name buffer.  Limit to 15 characters including extension */
        (len = strlen(t1)) <= (MAX_FILENAME_LEN - 3) ? len : (len=MAX_FILENAME_LEN - 3);
        strncpy(t3, t1, len);
        strncpy(p_buf, t3, len);

        /* Append "~" and file extension to long filenames only */
        if ( len >= (MAX_FILENAME_LEN - 3) )
        {
            strcat(p_buf,"~");
            strcat(p_buf,ext);
        }

        free(t3);
    }
}

/* Subsequent setlogmask call overrides any previous setlogmask call. */
void syslog_mask(int mask)
{
    setlogmask(LOG_UPTO(mask));
}


/* Sets syslog mask configured thru the command interface */
void set_global_mask(char* mask)
{
    assert(mask!=NULL);
    fp=fopen(LOG_FILE_PATH,"w+");
    if (fp)
    {
        fputs((char*)mask, fp);
        fclose(fp);
    }

    /* Set syslog mask locally */
    setlogmask(LOG_UPTO(atoi(mask)));

}

/* Retrieves syslog mask configured thru the command interface */
int get_global_mask()
{
    char buf[128]= {0};
    fp=fopen(LOG_FILE_PATH,"r");
    if (fp)
    {
        if (fread(&buf, 1, sizeof(buf), fp)!=0) {
            return (atoi(buf));
        }
    }

    return 0;
}

void slog0(int mask, const char* msg)
{
    char* p;
    char* t;

    assert(msg!=NULL);

    if (mask > get_global_mask())
    {
        return;
    }

    p=(char*)calloc(SIZE,sizeof(char));
    t=p;
    if (p)
    {
        char file_buf[SIZE]= {0};
        /* Omit trailing directory descriptors in file name and limit to 15 characters */
        snprintf(p, strlen(__FILE__)+1, "%s", __FILE__);
        strcpy(&file_buf[0],p);
        parse(&file_buf[0]);

        /* Print formatted message to syslog */
        syslog(mask, "[%s (%d)] %s", &file_buf[0], __LINE__, msg);
        free(t);
    }
}

void slog(int mask, const char* fmt, const char* file, int line, const char* format, ...)
{
    char* p;
    va_list args;
    va_start(args, format);

    assert(fmt!=NULL);
    assert(file!=NULL);
    assert(format!=NULL);

    if (mask > get_global_mask())
    {
        return;
    }

    p=(char*)calloc(SIZE,sizeof(char));
    if (p)
    {
        char buf[SIZE] = {0};
        char file_buf[SIZE] = {0};
        char* t=p;

        /* Copy message to buffer */
        int n = vsnprintf(buf, sizeof(buf), format, args);
        if (n == SIZE || n < 1)
        {
            perror("vsnprintf failed");
            free(t);
            return;
        }

        /* Omit trailing directory descriptors in file name and limit to 15 characters */
        snprintf(p, strlen(__FILE__)+1, "%s", __FILE__);
        strcpy(&file_buf[0],p);
        parse(&file_buf[0]);

        /* Print formatted message to syslog */
        syslog(mask, "[%s (%d)] %s", &file_buf[0], __LINE__, buf);

        free(t);
        va_end(args);
    }
}

void log_this0(const char* x, const char* msg, const char* file, int line)
{
    syslog(LOG_INFO, "[%s (%d)] %s", __FILE__, __LINE__, msg);
}

void log_this(const char* fmt, const char* file, int line, const char* format, ...)
{
    char buf[SIZE] = {0};
    va_list args;
    va_start(args, format);

    int n = vsnprintf(buf, sizeof(buf), format, args);

    if (n == SIZE || n < 1)
    {
        perror("vsnprintf failed");
    }
    else
    {
        buf[n - 1]= '\0';
        syslog(LOG_INFO, fmt, file, line, buf);
    }
    va_end(args);
}

