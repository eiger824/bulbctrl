#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void help()
{
    fprintf(stderr, "USAGE: ./togglebulb [1|0]\n");
}

int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        help();
        return -1;
    }

    int fd, err;
    char buffer[2];

    fd = open("/dev/bulbctrl", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open device");
        return errno;
    }

    // Get current state
    if ( (err = read(fd, buffer, 1)) < 0)
    {
        perror("Error reading");
    }
    buffer[1] = 0;
    printf("Current bulb state is: %s\n", (!strcmp(buffer,"1")?"ON":"OFF"));

    // Parse state and write it
    if (!strcmp(argv[1], "1"))
    {
        err = write(fd, "1", 1);
    }
    else if (!strcmp(argv[1], "0"))
    {
        err = write(fd, "0", 1);
    }
    else
    {
        help();
        close(fd);
        return -1;
    }
    
    if (err < 0)
    {
        perror("Error writing to device");
        close(fd);
        return errno;
    }
    close(fd);
    return 0;
}

