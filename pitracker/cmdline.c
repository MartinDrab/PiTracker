
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "logging.h"
#include "settings.h"
#include "cmdline.h"



typedef enum _EOptionType {
	otUnknown,
	otVersion,
	otVerbose,
	otHelp,
	otDevice,
	otBaudrate,
	otLogFile,
	otPIN,
	otConfigFile,
	otGPSFile,
	otMax,
} EOptionType, * PEOptionType;


typedef struct _OPTION_RECORD {
	EOptionType Type;
	int Present;
	const char* ShortName;
	const char* LongName;
	int ArgumentCount;
} OPTION_RECORD, * POPTION_RECORD;


static OPTION_RECORD _options[] = {
	{otHelp, 0, "h", "help", 0},
	{otVersion, 0, "v", "version", 0},
	{otVerbose, 0, "V", "verbose", 1},
	{otDevice, 0, "D", "device", 1},
	{otBaudrate, 0, "b", "baudrate", 1},
	{otLogFile, 0, "l", "log-file", 1},
	{otPIN, 0, "p", "pin", 1},
	{otConfigFile, 0, "c", "config-file", 1},
	{otGPSFile, 0, "g", "gps-file", 1},
};


int _help = 0;
int _version = 0;
char* _configFile = NULL;


static int _on_cmd_option(EOptionType Type, const char** Arguments, int ArgumentCount)
{
	int ret = 0;
	unsigned long baudrate = 0;
	const char* tmp = NULL;

	switch (Type) {
		case otHelp:
			_help = 1;
			break;
		case otVersion:
			_version = 1;
			break;
		case otDevice:
			ret = settings_value_set_string("device", 0, Arguments[0]);
			if (ret != 0)
				log_error("Unable to add (device,%s): %i", Arguments[0], ret);
			break;
		case otLogFile:
			ret = settings_value_set_string("logfile", 0, Arguments[0]);
			if (ret != 0)
				log_error("Unable to add (logfile,%s): %i", Arguments[0], ret);
			break;
		case otBaudrate:
			baudrate = strtoul(Arguments[0], (char**)&tmp, 0);
			if (tmp == NULL || *tmp != '\0' || baudrate == 0 || (baudrate == ULONG_MAX && errno == ERANGE)) {
				ret = -5;
				log_error("Invalid baud rate: %s", Arguments[0]);
				goto Exit;
			}

			if (baudrate > UINT_MAX) {
				ret = ERANGE;
				log_error("Invalid baud rate: %s", Arguments[0]);
				goto Exit;
			}

			ret = settings_value_set_string("baudrate", 0, Arguments[0]);
			if (ret != 0)
				log_error("Unable to add (baudrate,%s): %i", Arguments[0], ret);
			break;
		case otVerbose:
			_verbose = strtoul(Arguments[0], (char**)&tmp, 0);
			if (tmp == NULL || *tmp != '\0' || _verbose == 0 || (_verbose == ULONG_MAX && errno == ERANGE)) {
				ret = -6;
				log_error("Invalid verbosity: %s", Arguments[0]);
				goto Exit;
			}
			break;
		case otPIN:
			ret = settings_value_set_string("pin", 0, Arguments[0]);
			if (ret != 0)
				log_error("Unable to add (pin,%s): %i", Arguments[0], ret);
			break;
		case otConfigFile:
			_configFile = strdup(Arguments[0]);
			if (_configFile == NULL) {
				ret = ENOMEM;
				log_error("Unable to copy string: %i", ret);
				goto Exit;
			}

			ret = settings_load(_configFile, ':', '#');
			if (ret != 0) {
				free(_configFile);
				_configFile = NULL;
				log_error("Unable to parse the config file: %i", ret);
				goto Exit;
			}
			break;
		case otGPSFile:
			ret = settings_value_set_string("gpsfile", 0, Arguments[0]);
			if (ret != 0)
				log_error("Unable to add (gpsfile,%s): %i", Arguments[0], ret);
			break;
		default:
			ret = -7;
			goto Exit;
			break;
	}

Exit:
	return ret;
}


int process_command_line(int argc, char** argv)
{
	int ret = 0;
	int found = 0;
	size_t len = 0;
	int isLong = 0;
	int isShort = 0;
	char** argvIt = NULL;
	const char* arg = NULL;
	POPTION_RECORD opR = NULL;
	const char* stackArgs[64];

	argvIt = argv + 1;
	while (ret == 0 && argvIt - argv < argc) {
		isShort = 0;
		isLong = 0;
		found = 0;
		arg = *argvIt;
		len = strlen(arg);
		if (len == 2 && arg[0] == '-')
			isShort = 1;
		else if (len > 2 && memcmp(arg, "--", 2 * sizeof(char)) == 0)
			isLong = 1;

		if (!isShort && !isLong) {
			ret = -1;
			log_error("Invalid argument: %s", arg);
			break;
		}

		opR = _options;
		for (size_t i = 0; i < sizeof(_options) / sizeof(_options[0]); ++i) {
			if ((isShort && strcmp(opR->ShortName, arg + 1) == 0) ||
				(isLong && strcmp(opR->LongName, arg + 2) == 0)) {
				++opR->Present;
				if (opR->Present > 1) {
					ret = -2;
					log_error("Argument present more than once: %s", arg);
					break;
				}

				if (argvIt - argv + opR->ArgumentCount > argc) {
					ret = -3;
					log_error("Argument %s requires %i parameters, %i given", arg, opR->ArgumentCount, argc - (argvIt - argv + 1));
					break;
				}

				for (size_t j = 0; j < opR->ArgumentCount; ++j) {
					++argvIt;
					stackArgs[j] = *argvIt;
				}

				ret = _on_cmd_option(opR->Type, stackArgs, opR->ArgumentCount);
				if (ret == 0)
					found = 1;
			}

			if (ret != 0)
				break;

			++opR;
		}

		if (ret == 0 && !found) {
			ret = -4;
			log_error("Unknown option: %s", arg);
			break;
		}

		++argvIt;
	}

	return ret;
}
