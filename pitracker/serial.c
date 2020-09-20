
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <termios.h>
#include <limits.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <linux/serial.h>
#include <poll.h>
#include <unistd.h>
#include "logging.h"
#include "line-buffer.h"
#include "serial.h"


static int _line_buffer_OK_callback(const char* Line, void* Context)
{
	int ret = 0;
	int* pFound = NULL;

	pFound = (int*)Context;
	if (*pFound != -1) {
		*pFound = (
			strcmp(Line, "OK") == 0 ||
			strcmp(Line, "ERROR") == 0
			);
	}

	return ret;
}


static speed_t _rate_to_constant(int baudrate)
{
#define B(x) case x: return B##x
	switch (baudrate) {
		B(50);     B(75);     B(110);    B(134);    B(150);
		B(200);    B(300);    B(600);    B(1200);   B(1800);
		B(2400);   B(4800);   B(9600);   B(19200);  B(38400);
		B(57600);  B(115200); B(230400); B(460800); B(500000);
		B(576000); B(921600); B(1000000); B(1152000); B(1500000);
	}
#undef B

	return B0;
}


int serial_open(const char* device, int rate, int* Handle)
{
	int ret = 0;
	int fd = -1;
	speed_t speed = 0;
	struct termios options;
	log_enter("device=\"%s\"; rate=%u; Handle=0x%p", device, rate, Handle);

	fd = open(device, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		ret = errno;
		log_error("open(\"%s\"): %i", device, ret);
		goto Cleanup;
	}

	speed = _rate_to_constant(rate);
	if (speed == B0) {
		log_error("Unknown baud rate");
		goto Cleanup;
	}

	if (fcntl(fd, F_SETFL, 0) < 0) {
		ret = errno;
		log_error("fnctl(F_SETFL): %i", ret);
		goto Cleanup;
	}

	if (tcgetattr(fd, &options) == -1) {
		ret = errno;
		log_error("tcgetattr: %i", ret);
		goto Cleanup;
	}

	cfsetispeed(&options, speed);
	cfsetospeed(&options, speed);
	cfmakeraw(&options);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~CRTSCTS;
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		ret = errno;
		log_error("tcsetattr(TCSANOW): %i", ret);
		goto Cleanup;
	}

	*Handle = fd;
	fd = -1;
Cleanup:
	if (fd != -1)
		close(fd);

	log_exit("%i, *Handle=%i", ret, *Handle);
	return ret;
}


void serial_close(int Handle)
{
	log_enter("Handle=%i", Handle);

	close(Handle);

	log_exit("void");
	return;
}


int serial_command(int fd, const char *Command, int CR, int LF)
{
	int ret = 0;
	size_t len = 0;
	ssize_t transmitted = 0;
	char cmd[256];
	const char* tmp = NULL;
	log_enter("fd=%i; Command=\"%s\"", fd, Command);

	memset(cmd, 0, sizeof(cmd));
	strncpy(cmd, Command, sizeof(cmd) / sizeof(cmd[0]) - 3);
	len = strlen(cmd);
	if (CR) {
		cmd[len] = '\r';
		++len;
	}

	if (LF) {
		cmd[len] = '\n';
		++len;
	}

	tmp = cmd;
	while (len > 0) {
		transmitted = write(fd, tmp, len);
		if (transmitted == -1) {
			ret = errno;
			log_error("Unable to write data", ret);
			break;
		}

		len -= (size_t)transmitted;
		tmp += transmitted;
	}

	log_exit("%i", ret);
	return ret;
}


int serial_response_wait(int fd, int Timeout, int OKSearch, char **Response, size_t *ResponseSize)
{
	int ret = 0;
	char* tmpResponse = NULL;
	char* newResponse = NULL;
	size_t tmpResponseSize = 0;
	ssize_t transmitted = 0;
	struct pollfd fds;
	char buf[1024];
	void* okCallbackHandle = NULL;
	int okFound = 0;
	log_enter("fd=%i; Timeout=%i; OKSearch=%u; Response=0x%p; ResponseSize=0x%p", fd, Timeout, OKSearch, Response, ResponseSize);

	if (!OKSearch)
		okFound = -1;

	ret = line_callback_register(_line_buffer_OK_callback, &okFound, &okCallbackHandle);
	if (ret == 0) {
		memset(&fds, 0, sizeof(fds));
		fds.fd = fd;
		fds.events = POLLIN;
		do {
			transmitted = 0;
			fds.revents = 0;
			ret = poll(&fds, sizeof(fds) / sizeof(fds), Timeout * 1000);
			switch (ret) {
			case 0:
				break;
			case -1:
				ret = errno;
				if (ret == EINTR) {
					ret = 0;
					transmitted = 1;
					Timeout -= 1;
					log_warning("poll() interrupted");
				}
				break;
			default:
				ret = 0;
				if (fds.revents & POLLERR) {
					ret = -1;
					log_error("Serial port error");
					continue;
				}

				if (fds.revents & POLLIN) {
					transmitted = read(fd, buf, sizeof(buf));
					if (transmitted == -1) {
						ret = errno;
						log_error("Unable to read data: %i", ret);
						continue;
					}

					ret = line_buffer_insert(buf, (size_t)transmitted);
					if (ret != 0) {
						log_error("Cannot insert %zu bytes into the Line Buffer: %i", ret);
						continue;
					}

					newResponse = realloc(tmpResponse, tmpResponseSize + (size_t)transmitted + 1);
					if (newResponse == NULL) {
						ret = ENOMEM;
						log_error("Unable to reallocate response buffer", ret);
						continue;
					}

					tmpResponse = newResponse;
					memcpy(tmpResponse + tmpResponseSize, buf, (size_t)transmitted);
					tmpResponseSize += (size_t)transmitted;
					tmpResponse[tmpResponseSize] = '\0';
				}

				if (fds.revents & POLLHUP) {
					transmitted = 0;
					log_info("HUP from the serival port");
					continue;
				}
				break;
			}
		} while (ret == 0 && transmitted > 0 && okFound != 1);
	
		line_callback_unregister(okCallbackHandle);
	}

	if (ret == 0 && Response != NULL) {
		*Response = tmpResponse;
		*ResponseSize = tmpResponseSize;
	}

	if (ret != 0 || Response == NULL) {
		free(tmpResponse);
		tmpResponse = NULL;
	}

	log_exit("%i, *Response=\"%s\", *ResponseSize=%zu", ret, tmpResponse, tmpResponseSize);
	return ret;
}


int serial_command_with_response(int fd, const char *Command, int CR, int LF, int OKSearch, char **Response, size_t* ResponseSize)
{
	int ret = 0;
	log_enter("fd=%i; Command=\"%s\"; CR=%u; LF=%i; OKSearch=%i; Response=0x%p; ResponseSize=0x%p", fd, Command, CR, LF, OKSearch, Response, ResponseSize);

	ret = serial_command(fd, Command, CR, LF);
	if (ret == 0)
		ret = serial_response_wait(fd, 4, OKSearch, Response, ResponseSize);

	log_exit("%i, *Response=\"%s\", *ResponseSize=%zu", ret, *Response, *ResponseSize);
	return ret;
}


int serial_response_to_lines(char* Response, size_t ResponseSize, char*** Lines, size_t* LineCount)
{
	int ret = 0;
	int lineEnding = 0;
	size_t tmpLineCount = 0;
	char* lineStart = NULL;
	char** tmp = NULL;
	char** tmpLines = *Lines;
	log_enter("Response=0x%p; ResponseSize=%zu; Lines=0x%p; LineCount=0x%p", Response, ResponseSize, Lines, LineCount);

	lineStart = Response;
	while (ret == 0 && *Response != '\0') {
		switch (*Response) {
		case 13:
			lineEnding = 1;
			break;
		case 10:
			if (lineEnding) {
				*Response = '\0';
				*(Response - 1) = '\0';
				if (tmpLineCount == *LineCount) {
					tmp = realloc(tmpLines, (tmpLineCount + 1) * sizeof(char*));
					if (tmp == NULL) {
						ret = ENOMEM;
						continue;
					}

					tmpLines = tmp;
					*LineCount = tmpLineCount + 1;
				}

				tmpLines[tmpLineCount] = lineStart;
				++tmpLineCount;
				lineEnding = 0;
				lineStart = Response + 1;
			}
			break;
		default:
			break;
		}

		++Response;
	}

	if (ret == 0 && lineStart != Response) {
		if (tmpLineCount == *LineCount) {
			tmp = realloc(tmpLines, (tmpLineCount + 1) * sizeof(char*));
			if (tmp != NULL) {
				tmpLines = tmp;
				*LineCount = tmpLineCount + 1;
			} else ret = ENOMEM;
		}

		if (ret == 0) {
			tmpLines[tmpLineCount] = lineStart;
			++tmpLineCount;
		}

		if (ret != 0)
			free(tmpLines);
	}

	if (ret == 0) {
		*Lines = tmpLines;
		*LineCount = tmpLineCount;
	}

	log_exit("%i, *Lines=0x%p, *LineCount=%zu", ret, *Lines, *LineCount);
	return ret;
}


ESerialCommandStatus serial_command_status(char** Lines, size_t LineCount)
{
	const char* l = NULL;
	size_t len = 0;
	ESerialCommandStatus ret = scsUnknown;
	log_enter("Lines=0x0x%p; LineCount=%zu", Lines, LineCount);

	for (size_t i = 0; i < LineCount; ++i) {
		l = Lines[i];
		len = strlen(l);
		if (strcmp(l, "OK") == 0) {
			ret = scsOK;
			break;
		} else if (strcmp(l, "SEND OK") == 0) {
			ret = scsOK;
			break;
		} else if (strcmp(l, "CLOSE OK") == 0) {
			ret = scsOK;
			break;
		} else if (strcmp(l, "SHUT OK") == 0) {
			ret = scsOK;
			break;
		} else if (strcmp(l, "ERROR") == 0) {
			ret = scsError;
			break;
		} else if (sizeof("+CME ERROR: ") - 1 <= len && memcmp(l, "+CME ERROR: ", sizeof("+CME ERROR: ") - 1) == 0) {
			ret = scsError;
			break;
		}
	}

	log_exit("%i", ret);
	return ret;
}


int serial_command_contains(char** Lines, size_t LineCount, const char* Value)
{
	int ret = 0;
	log_enter("Lines=0x%p; LineCount=%zu", Lines, LineCount);

	for (size_t i = 0; i < LineCount; ++i) {
		if (strcmp(Lines[i], Value) == 0) {
			ret = 1;
			break;
		}
	}

	log_exit("%i", ret);
	return ret;
}
