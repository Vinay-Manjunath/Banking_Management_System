#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "utils.h"

// Reads a single line from file descriptor into buffer
int read_line(int fd, char *buffer) {
    char c;
    int i = 0;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n' || c == '\0') {
            buffer[i] = '\0';
            return 1;
        }
        buffer[i++] = c;
    }
    return (i > 0);
}

// Removes trailing newline or carriage return from string
void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
        str[len - 1] = '\0';
}