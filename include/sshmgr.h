#ifndef SSHMGR_H
#define SSHMGR_H

#include <ctype.h>
#include <errno.h>
#include <libssh/libssh.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>



#define PROMPT "^([A-Za-z0-9_.-]+)(\\(config[^(]*\\))?[>#]\\s*$"
#define KEX "diffie-hellman-group14-sha256,ecdh-sha2-nistp256"
#define BUF_SIZE 4096
#define PROGNAME "ccpumon"


typedef struct {
    char *host;
    char *user;
    size_t num_cmds;
    char **cmdlist;
    size_t timeout;
} SshArgs;

// Stores CLI user options
typedef struct {
    char     *host;
    char     *username;
} CommandLineArgs;



// Error messages and exit/return codes
#define ERROR_FAIL(fmt, ...)                                                    \
    do {                                                                        \
        fprintf(stderr, "[ERROR] file %s, function %s(), line %d: ",            \
                strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__, \
                __func__, __LINE__ -1 );                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__);                                    \
        fprintf(stderr, "\n");                                                  \
        fflush(stderr);                                                         \
        exit(EXIT_FAILURE);                                                     \
    } while (0)


#define RETURN_INT_MSG(fmt, ...)                                                \
    do {                                                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__);                                    \
        fprintf(stderr, "\n");                                                  \
        fflush(stderr);                                                         \
        return -1;                                                              \
    } while (0)

#define ERROR_RETURN_NULL(fmt, ...)                                             \
    do {                                                                        \
        fprintf(stderr, "[ERROR] file %s, function %s(), line %d: ",            \
                strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__, \
                __func__, __LINE__ -1 );                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__);                                    \
        fprintf(stderr, "\n");                                                  \
        fflush(stderr);                                                         \
        return NULL;                                                            \
    } while (0)

#define RETURN_NULL                                                               \
    do {                                                                          \
        fprintf(stderr, "[ERROR] file %s, function %s(), line %d: return NULL\n", \
                strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 :             \
                __FILE__, __func__, __LINE__ - 1);                                \
        return NULL;                                                              \
    } while(0)

#define RETURN_VOID                                                           \
    do {                                                                      \
        fprintf(stderr, "[ERROR] file %s, function %s(), line %d: return\n",  \
                strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 :         \
                __FILE__, __func__, __LINE__ - 1);                            \
        return;                                                               \
    } while(0)

#define RETURN_INT                                                              \
    do {                                                                        \
        fprintf(stderr, "[ERROR] file %s, function %s(), line %d: return -1\n", \
                strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 :           \
                __FILE__, __func__, __LINE__);                                  \
        return -1;                                                              \
    } while(0)

// functiion declarations
void siginthdlr(int);
void ssh_main(char*, char*);
int ssh_exec(ssh_session, SshArgs*, regex_t*);
char* ssh_read(ssh_channel, regex_t*);
void display_cpu(char*, char*);
regex_t compile_re(const char*);
void clean_output(char*);
char *remove_prompt(const char*, const char*);
void usage();

#endif
