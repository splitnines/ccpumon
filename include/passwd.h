#ifndef PASSWD_H
#define PASSWD_H

#include <termios.h>
#include <stdlib.h>


/**
 * Reads a password from the standard input and stores it in the provided
 * buffer.
 * 
 * @param getpasswd A pointer to a character array where the password will be
 * stored
 * @param passwd_size The maximum number of characters that can be read for the
 * password
 * 
 * @return void
 * 
 * This function reads a password from the standard input. If the standard
 * input is a terminal, it prompts the user to enter a password without echoing
 * the characters to the terminal. If the standard input is not a terminal, it
 * reads the password normally. The password is stored in the provided buffer
 * 'getpasswd' with a maximum length of 'passwd_size' characters. If the
 * password is longer than 'passwd_size', it is truncated to fit. The function
 * ensures that the newline character at the end of the password is removed.
 * 
 * If an error occurs while reading the password, the function prints an error
 * message to stderr and exits with a failure status.
 */
extern void passwd(char *password, size_t size);

#endif
