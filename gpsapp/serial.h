
#pragma once


typedef enum _ESerialCommandStatus {
	scsUnknown,
	scsOK,
	scsError,
} ESerialCommandStatus, * PESerialCommandStatus;


int serial_open(const char* device, int rate, int* Handle);
void serial_close(int Handle);
int serial_command(int fd, const char *Command, int CR, int LF);
int serial_response_wait(int fd, int Timeout, int OKSearch, char** Response, size_t* ResponseSize);
int serial_command_with_response(int fd, const char* Command, int CR, int LF, int OKSearch, char** Response, size_t* ResponseSize);
int serial_response_to_lines(char* Response, size_t ResponseSize, char*** Lines, size_t* LineCount);
ESerialCommandStatus serial_command_status(char** Lines, size_t LineCount);
int serial_command_contains(char** Lines, size_t LineCount, const char* Value);
