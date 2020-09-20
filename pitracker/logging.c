
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "logging.h"


unsigned long _verbose = 0x3;



void LogMessage(ELogType Type, const char* Format, ...)
{
	va_list vl;
	char msg[1024];

	if (_verbose & (1 << (int)Type)) {
		va_start(vl, Format);
		memset(msg, 0, sizeof(msg));
		vsnprintf(msg, sizeof(msg) / sizeof(msg[0]), Format, vl);
		fputs(msg, stderr);
		va_end(vl);
	}

	return;
}
