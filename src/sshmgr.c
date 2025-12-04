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


void validate_args(SshArgs *sshargs)
{
    if (strlen(sshargs->host) > 256) {
        fprintf(stderr, "Invalid host/IP address: input too long\n");
        exit(1);
    }

    if (strlen(sshargs->user) > 64) {
        fprintf(stderr, "Invalid username: input too long\n");
        exit(1);
    }
}


int ssh_main(char *host, char *username)
{
    struct sigaction sa;
    sa.sa_handler = siginthdlr;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    char *commands[2] = {
        "show process cpu history\n", 
        "show clock\n"
    };

    SshArgs sshargs = {
        .host = host,
        .user = username,
        .num_cmds = sizeof(commands) / sizeof(commands[0]),
        .cmdlist = commands,
        .timeout = 2
    };

    validate_args(&sshargs);

    ssh_session sess = ssh_new();
    if (sess == NULL)
        ERROR_FAIL("Failed to create ssh session");

    if (sshargs.host[strlen(sshargs.host) - 1] == '\n')
        sshargs.host[strlen(sshargs.host) - 1] = '\0';

    ssh_options_set(sess, SSH_OPTIONS_HOST, sshargs.host);
    ssh_options_set(sess, SSH_OPTIONS_USER, sshargs.user);
    ssh_options_set(sess, SSH_OPTIONS_TIMEOUT, &sshargs.timeout);
    ssh_options_set(sess, SSH_OPTIONS_KEY_EXCHANGE, KEX);

    if (ssh_connect(sess) != SSH_OK) {
        fprintf(stderr, "Conneciton failed: (%s)\n", ssh_get_error(sess));
        ssh_free(sess);
        exit(1);
    }

    const size_t max_pw_len = 66;
    char *password = malloc(max_pw_len);
    if (!password)
        EXIT_FAILURE;
    passwd(password, max_pw_len);


    if (ssh_userauth_password(sess, NULL, password) != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Authentication failed: (%s)\n", ssh_get_error(sess));
        ssh_disconnect(sess);
        ssh_free(sess);
        exit(1);
    }

    memset(password, 0, max_pw_len + 1);
    free(password);

    regex_t prompt_re = compile_re(PROMPT);

    if (sshargs.num_cmds > 0) {
        if (ssh_exec(sess, &sshargs, &prompt_re) == -1) {
            ssh_disconnect(sess);
            ssh_free(sess); sess = NULL;
            regfree(&prompt_re);
            exit(1);
        }
    }

    ssh_disconnect(sess);

    ssh_free(sess); sess = NULL;
    regfree(&prompt_re);

    return 0;
}


void display_cpu(char *cpu_reading, char *host)
{
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
 
    clean_output(cpu_reading);
 
    printf("%shost: %s", cpu_reading, host);
    fflush(stdout);
}


int ssh_exec(ssh_session sess, SshArgs *sshargs, regex_t *prompt_re)
{
    ssh_channel channel = ssh_channel_new(sess);
    if (channel == NULL)
        RETURN_INT;

    if (ssh_channel_open_session(channel) != SSH_OK)
        RETURN_INT;

    if (ssh_channel_request_shell(channel) != SSH_OK)
        RETURN_INT;

    // Disable the paging on the cli
    char *disablepaging[] = {
        "terminal length 0\n",
        "terminal width 0\n"
    };

    char *tmp;
    size_t n = sizeof(disablepaging) / sizeof(disablepaging[0]);
    for (size_t i = 0; i < n; i++) {
        ssh_channel_write(channel, disablepaging[i], strlen(disablepaging[i]));
        tmp = ssh_read(channel, prompt_re);
        if (!tmp) {
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            free(tmp);
            return -1;
        }
        free(tmp); tmp = NULL;
    }
    while (1) {
        if (stop_flag)
            break;

        char *all_results = malloc(1);
        if (all_results == NULL) {
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel); channel = NULL;
            return -1;
        }
        (all_results)[0] = '\0';

        for (size_t i = 0; i < sshargs->num_cmds; ++i) {
            ssh_channel_write(channel, sshargs->cmdlist[i], strlen(sshargs->cmdlist[i]));

            char *raw_results = ssh_read(channel, prompt_re);
            if (raw_results == NULL) {
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                free(all_results);
                return -1;
            }

            size_t used = strlen(all_results);
            size_t add  = strlen(raw_results);
 
            char *newptr = realloc(all_results, used + add + 1);
            if (!newptr) {
                free(raw_results);
                free(all_results);
                return -1;
            }
            all_results = newptr;

            char *cleaned_results = remove_prompt(raw_results, PROMPT);
            if (!cleaned_results) {
                free(all_results);
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return -1;
            }

            free(raw_results); raw_results = NULL;

            strcat(all_results, cleaned_results);
            free(cleaned_results); cleaned_results = NULL;

        }

        ssh_channel_write(channel, "\n", 1);

        char *raw_final_output = ssh_read(channel, prompt_re);
        if (!raw_final_output) {
            free(all_results);
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return -1;
        }

        char *cleaned_final_output = remove_prompt(raw_final_output, PROMPT);
        free(raw_final_output); raw_final_output = NULL;

        if (cleaned_final_output) {
            size_t used = strlen(all_results);
            size_t add  = strlen(cleaned_final_output);

            char *tmp = realloc(all_results, used + add + 1);
            if (!tmp) {
                free(all_results);
                free(cleaned_final_output);
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return -1;
            }

            all_results = tmp;
            strcat(all_results, cleaned_final_output);
            free(cleaned_final_output); cleaned_final_output = NULL;
        }
        display_cpu(all_results, sshargs->host);

        free(all_results); all_results = NULL;

        sleep(1);
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel); channel = NULL;

    puts("\n");
    return 0;
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
            if (buffer) free(buffer);
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

        // nbytes == 0;
        if (nbytes == SSH_OK) {
            if (++idlecnt > MAXIDLE)
                break;
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

    const char *pinput = input;
    const char *newline;
    char *out  = NULL;
    size_t out_size = 0;
    size_t out_used = 0;

    char line[BUF_SIZE];

    while (*pinput) {
        newline = strchr(pinput, '\n');
        size_t len = newline ? (size_t)(newline - pinput) : strlen(pinput);

        if (len >= sizeof(line))
            len = sizeof(line) - 1;

        memcpy(line, pinput, len);
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
        pinput = newline ? newline + 1 : pinput + len;
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


void usage()
{
    fprintf(stderr, "Usage: %s <host name or IP address> <username>", PROGNAME);
}
