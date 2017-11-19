#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

void help()
{
	fprintf(stderr, "USAGE: ./togglebulb g|t 1|0\n");
}

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		help();
		return -1;
	}


	int c;
	int set = 1;
	char *parse_state = NULL;

	while ((c = getopt(argc, argv, "gt:")) != -1)
	{
		switch(c)
		{
			case 'g':
				set = 0;
				break;
			case 't':
				set = 1;
				parse_state = optarg;
				break;
			default:
				help();
				return -1;
		}
	}

	int fd, err, state;
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
	state = atoi(buffer);
	if (!set)
	{
		printf("%d\n", state);
		close(fd);
		return 0;
	}

	// Parse state and write it
	int new_state = atoi(parse_state);
	if (new_state == 1)
	{
		if (!state)
		{
			printf("Switching ON!\n");
			err = write(fd, "1", 1);
		}
	}
	else if (new_state == 0)
	{
		if (state)
		{
			printf("Switching OFF!\n");
			err = write(fd, "0", 1);
		}
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

