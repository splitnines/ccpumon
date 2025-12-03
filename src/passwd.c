#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>



// Function to disable echo
static void echo_off(struct termios *passwd_old_settings) {

    if (!isatty(STDIN_FILENO)) {
        return;
    }

    struct termios passwd_new_settings;

    if (tcgetattr(STDIN_FILENO, passwd_old_settings) == -1) {
        perror("tcgetattr");
        return;
    }

    passwd_new_settings = *passwd_old_settings;
    passwd_new_settings.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &passwd_new_settings) == -1) {
        perror("tcsetattr");
        return;
    }
}


// Function to restore echo
static void echo_on(const struct termios *passwd_old_settings) {

    if (!isatty(STDIN_FILENO)) {
        return;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, passwd_old_settings) == -1) {
        perror("tcsetattr");
    }
}


extern void passwd(char *getpasswd, size_t passwd_size) {

    struct termios passwd_old_setting;

    if (isatty(STDIN_FILENO)) {
        fputs("Password:", stdout);
        fflush(stdout);

        echo_off(&passwd_old_setting);

        if (fgets(getpasswd, passwd_size, stdin) == NULL) {
            fputs("\nFailed to get getpasswd.\n", stderr);
            echo_on(&passwd_old_setting);
            exit(EXIT_FAILURE);
        }

        getpasswd [strcspn(getpasswd, "\n")] = '\0';
        echo_on(&passwd_old_setting);
        fputs("\n", stdout);

    } else {

        if (fgets(getpasswd, passwd_size, stdin) == NULL) {
            perror("Failed to get getpasswd\n");
            exit(EXIT_FAILURE);
        }

        getpasswd[strcspn(getpasswd, "\n")] = '\0';

    }
}
