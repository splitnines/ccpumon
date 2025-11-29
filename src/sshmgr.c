#include <getopt.h>
#include <stdbool.h>
#include <libssh/libssh.h>
#include <regex.h>
#include <stdlib.h>
#include <signal.h>

#include "../include/sshmgr.h"
#include "../include/passwd.h"


volatile sig_atomic_t stop_flag = 0;

void siginthdlr(int sig)
{
    (void)sig;
    stop_flag = 1;
}


void ssh_main(char *host, char *username)
{
    struct sigaction sa;
    sa.sa_handler = siginthdlr;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    char password[128];
    passwd(password, sizeof(password));

    char *commands[2] = {"show process cpu history\n", "show clock\n"};
    SshArgs sshargs = {
        .host = host,
        .user = username,
        .password = password,
        .num_cmds = 2,
        .cmdlist = commands,
        .timeout = 2
    };
    SshArgs *psshargs = &sshargs;

    ssh_session sess = ssh_new();
    if (sess == NULL)
        ERROR_FAIL("Failed to create ssh session");

    if (psshargs->host[strlen(psshargs->host) - 1] == '\n')
        psshargs->host[strlen(psshargs->host) - 1] = '\0';

    ssh_options_set(sess, SSH_OPTIONS_HOST, psshargs->host);
    ssh_options_set(sess, SSH_OPTIONS_USER, psshargs->user);
    ssh_options_set(sess, SSH_OPTIONS_TIMEOUT, &psshargs->timeout);
    ssh_options_set(sess, SSH_OPTIONS_KEY_EXCHANGE, KEX);

    if (ssh_connect(sess) != SSH_OK) {
        fprintf(stderr, "Conneciton failed: (%s)\n", ssh_get_error(sess));
        ssh_free(sess);
        exit(1);
    }

    if (ssh_userauth_password(sess, NULL, psshargs->password) != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Authentication failed: (%s)\n", ssh_get_error(sess));
        ssh_disconnect(sess);
        ssh_free(sess);
        exit(1);
    }

    regex_t prompt_re = compile_re(PROMPT);

    char *result = NULL;
    if (psshargs->num_cmds > 0) {
        if (ssh_exec(sess, psshargs->cmdlist, psshargs->num_cmds, &prompt_re,
                     result, psshargs->host) == -1) {
            ssh_disconnect(sess);
            ssh_free(sess); sess = NULL;
            regfree(&prompt_re);
            exit(1);
        }
    }
    ssh_disconnect(sess);

    ssh_free(sess); sess = NULL;
    regfree(&prompt_re);
}


int ssh_exec(ssh_session sess, char **cmds, size_t numcmds, regex_t *prompt_re,
             char *allresults, char *host)
{
    ssh_channel channel = ssh_channel_new(sess);
    if (channel == NULL)
        RETURN_INT;

    if (ssh_channel_open_session(channel) != SSH_OK)
        RETURN_INT;

    if (ssh_channel_request_shell(channel) != SSH_OK)
        RETURN_INT;

    // Disable the paging on the cli
    char *disablepaging[] = {"terminal length 0\n", "terminal width 0\n"};
    char *tmp;
    size_t n = sizeof(disablepaging) / sizeof(disablepaging[0]);
    for (size_t i = 0; i < n; i++) {
        ssh_channel_write(channel, disablepaging[i], strlen(disablepaging[i]));
        tmp = ssh_read(channel, prompt_re);
        free(tmp); tmp = NULL;
    }
    while (1) {
        if (stop_flag) {
            break;
        }

        allresults = malloc(1);
        if (allresults == NULL) {
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel); channel = NULL;
            return -1;
        }
        (allresults)[0] = '\0';

        for (size_t i = 0; i < numcmds; ++i) {
            if (stop_flag) {
                if (allresults) free(allresults);
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return 0;
            }

            ssh_channel_write(channel, cmds[i], strlen(cmds[i]));

            char *raw_results = ssh_read(channel, prompt_re);
            if (raw_results == NULL) {
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                free(allresults);
                return -1;
            }

            size_t used = strlen(allresults);
            size_t add  = strlen(raw_results);
 
            char *newptr = realloc(allresults, used + add + 1);
            if (!newptr) {
                free(raw_results);
                free(allresults);
                return -1;
            }
            allresults = newptr;

            char *clean_results = remove_prompt(raw_results, PROMPT);
            if (!clean_results) {
                if (allresults) free(allresults);
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return -1;
            }

            free(raw_results); raw_results = NULL;

            strcat(allresults, clean_results);
            free(clean_results); clean_results = NULL;

        }

        if (stop_flag) {
            free(allresults);
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return 0;
        }

        ssh_channel_write(channel, "\n", 1);

        char *raw_final_output = ssh_read(channel, prompt_re);
        if (!raw_final_output) {
            free(allresults);
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return -1;
        }

        char *cleaned_final_output = remove_prompt(raw_final_output, PROMPT);
        free(raw_final_output); raw_final_output = NULL;

        if (cleaned_final_output) {
            size_t used = strlen(allresults);
            size_t add  = strlen(cleaned_final_output);

            char *tmp = realloc(allresults, used + add + 1);
            if (!tmp) {
                free(allresults);
                free(cleaned_final_output);
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return -1;
            }

            allresults = tmp;
            strcat(allresults, cleaned_final_output);
            free(cleaned_final_output); cleaned_final_output = NULL;
        }
        display_cpu(allresults, host);

        free(allresults); allresults = NULL;

        sleep(1);
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel); channel = NULL;

    return 0;
}


void display_cpu(char *cpu_reading, char *host)
{
    printf("\033[2J\033[H");
    fflush(stdout);
 
    clean_output(cpu_reading);
 
    printf("%s\nhost: %s\n", cpu_reading, host);
}


char *ssh_read(ssh_channel channel, regex_t *prompt_re)
{
    size_t bufsize = BUF_SIZE;
    char *buffer = (char*)malloc(sizeof(char) * bufsize);
    if (buffer == NULL)
        RETURN_NULL;

    size_t totalbytes   = 0;
    const int readbytes = 1024;
    int idlecnt         = 0;
    const int MAXIDLE   = 120;
    const int TIMEOUTMS = 1000;

    while (1) {
        if (stop_flag) {
            free(buffer);
            return NULL;
        }

        if (totalbytes + readbytes >= bufsize) {
            bufsize *= 2;
            char *tmp = (char*)realloc(buffer, sizeof(char) * bufsize);
            if (tmp == NULL) {
                free(buffer);
                RETURN_NULL;
            }
            buffer = tmp;
        }
        int nbytes = ssh_channel_read_timeout(channel, buffer + totalbytes,
                                              readbytes, 0, TIMEOUTMS);

        if (nbytes == SSH_EOF)
            break;

        if (nbytes == SSH_ERROR) {
            free(buffer); buffer = NULL;
            RETURN_NULL;
        }

        if (nbytes == 0) {
            if (++idlecnt > MAXIDLE) {
                break;
            }
            continue;
        }
        idlecnt = 0;
        totalbytes += nbytes;
        buffer[totalbytes] = '\0';

        if (regexec(prompt_re, buffer, 0, NULL, 0) == 0)
            break;
    }
    buffer[totalbytes] = '\0';
    return buffer;
}


regex_t compile_re(const char *pattern)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NEWLINE) != 0)
        ERROR_FAIL("failed to compile regex: %s", pattern);

    return re;
}

void clean_output(char *all_output)
{
    char *remove[] = {
        "terminal length 0",
        "terminal width 0",
        "Enter configuration commands, one per line.  End with CNTL/Z.",
        "show process cpu history",
        "show clock"
    };

    size_t sz = sizeof(remove) / sizeof(remove[0]);
    for (size_t i = 0; i < sz; i++) {
        char *ptr;
        while ((ptr = strstr(all_output, remove[i])) != NULL) {
            memmove(ptr, ptr + strlen(remove[i]),
                    strlen(ptr + strlen(remove[i])) + 1);
        }
    }
}

char *remove_prompt(const char *input, const char *pattern)
{
    const char *marker = "last 60 seconds";

    regex_t regx;
    if (regcomp(&regx, pattern, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) != 0)
        return NULL;

    const char *p = input;
    const char *newline;
    char *out  = NULL;
    size_t out_size = 0;
    size_t out_used = 0;

    char line[BUF_SIZE];

    while (*p) {
        newline = strchr(p, '\n');
        size_t len = newline ? (size_t)(newline - p) : strlen(p);

        if (len >= sizeof(line))
            len = sizeof(line) - 1;

        memcpy(line, p, len);
        line[len] = '\0';

        if (strstr(line, marker) != NULL) {
            size_t need = len + 1;
            if (out_used + need + 1 > out_size) {
                size_t new_size = (out_size == 0) ? BUF_SIZE : out_size * 2;
                char *tmp = realloc(out, new_size);
                if (!tmp) {
                    free(out);
                    regfree(&regx);
                    return NULL;
                }
                out = tmp;
                out_size = new_size;
            }
            memcpy(out + out_used, line, len);
            out_used += len;
            out[out_used++] = '\n';
            out[out_used] = '\0';

            break;
        }

        int match = regexec(&regx, line, 0, NULL, 0);

        if (match == REG_NOMATCH) {
            size_t need = len + 1;
            if (out_used + need + 1 > out_size) {
                size_t new_size = (out_size == 0) ? BUF_SIZE : out_size * 2;
                char *tmp = realloc(out, new_size);
                if (!tmp) {
                    free(out);
                    regfree(&regx);
                    return NULL;
                }
                out = tmp;
                out_size = new_size;
            }

            memcpy(out + out_used, line, len);
            out_used += len;
            out[out_used++] = '\n';
            out[out_used] = '\0';
        }
        p = newline ? newline + 1 : p + len;
    }
    regfree(&regx);

    if (out == NULL) {
        out = malloc(1);
        if (!out) {
            return NULL;
        }
        out[0] = '\0';
    }
    return out;
}


// TODO: This needs to be completed.
void usage()
{
    fprintf(stderr, "Usage: %s <host name or IP address> <username>", PROGNAME);
}
