
#define _DEFAULT_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include "logging.h"
#include "field-array.h"
#include "settings.h"

/*

account: <number> <password> [admin=0|1]
account: * <password> [admin=0|1]
gps: 0|1
loc: <lat> <long> <timestamp>
logerror: 0|1
logwarning: 0|1
logtrace: 0|1
loginfo: 0|1
apn: <URL> <user> <password>
gprs: 0|1
name: <string>
batteryalarm: <percentage>
period: <seconds>
fence: <lat> <loc> <radius>
server: <ip> <port>
logfile: <filename>
maxloglines: <integer>
pin: <string>
device: </dev/ttyS0>
baudrate: <integer>
*/




typedef struct _SETTINGS_KEY_ENTRY {
	char *Name;
	size_t ValueCount;
	char **Values;
} SETTINGS_KEY_ENTRY, *PSETTINGS_KEY_ENTRY;

typedef struct _SETTINGS_ROOT_ENTRY {
	size_t KeyCount;
	PSETTINGS_KEY_ENTRY Keys;
} SETTINGS_ROOT_ENTRY, * PSETTINGS_ROOT_ENTRY;


static SETTINGS_ROOT_ENTRY _root;


static PSETTINGS_KEY_ENTRY _get_key_entry(const SETTINGS_ROOT_ENTRY* Root, const char *Key)
{
	PSETTINGS_KEY_ENTRY ret = NULL;
	PSETTINGS_KEY_ENTRY tmp = NULL;
	log_enter("Root=0x%p; Key=\"%s\"", Root, Key);

	tmp = Root->Keys;
	for (size_t i = 0; i < Root->KeyCount; ++i) {
		if (strcmp(Key, tmp->Name) == 0) {
			ret = tmp;
			break;
		}

		++tmp;
	}

	log_exit("0x%p", ret);
	return ret;
}


int settings_keys_enum(char*** Keys, size_t* Count)
{
	int ret = 0;
	size_t tmpCount = 0;
	char** tmpKeys = NULL;
	PSETTINGS_KEY_ENTRY entry = NULL;
	log_enter("Keys=0x%p; Count=0x%p", Keys, Count);

	tmpCount = _root.KeyCount;
	if (tmpCount > 0) {
		tmpKeys = calloc(tmpCount, sizeof(char*));
		if (tmpKeys != NULL) {
			entry = _root.Keys;
			for (size_t i = 0; i < tmpCount; ++i) {
				tmpKeys[i] = entry->Name;
				++entry;
			}
		} else ret = ENOMEM;
	}

	if (ret == 0) {
		*Keys = tmpKeys;
		*Count = tmpCount;
	}

	log_exit("%i, *Keys=0x%p, *Count=%zu", ret, *Keys, *Count);
	return ret;
}


int settings_key_add(const char* Key)
{
	int ret = 0;
	PSETTINGS_KEY_ENTRY entry = NULL;
	PSETTINGS_KEY_ENTRY newEntries = NULL;
	log_enter("Key=\"%s\"", Key);

	entry = _get_key_entry(&_root, Key);
	if (entry == NULL) {
		newEntries = realloc(_root.Keys, (_root.KeyCount + 1)*sizeof(SETTINGS_KEY_ENTRY));
		if (newEntries != NULL) {
			_root.Keys = newEntries;
			entry = newEntries + _root.KeyCount;
			memset(entry, 0, sizeof(SETTINGS_KEY_ENTRY));
			entry->Name = strdup(Key);
			if (entry->Name != NULL)
				++_root.KeyCount;
			else ret = ENOMEM;
		} else ret = ENOMEM;
	} else ret = EEXIST;

	log_exit("%i", ret);
	return ret;
}


int settings_key_delete(const char* Key)
{
	int ret = 0;
	PSETTINGS_KEY_ENTRY tmp = NULL;
	log_enter("Key=\"%s\"", Key);

	ret = ENOENT;
	tmp = _root.Keys;
	for (size_t i = 0; i < _root.KeyCount; ++i) {
		if (strcmp(Key, tmp->Name) == 0) {
			memmove(tmp, tmp + 1, (_root.KeyCount - i - 1)*sizeof(SETTINGS_KEY_ENTRY));
			--_root.KeyCount;
			ret = 0;
			break;
		}

		++tmp;
	}

	log_exit("0x%p", ret);
	return ret;
}


void settings_keys_free(char** Keys, size_t Count)
{
	log_enter("Keys=0x%p; Count=%zu", Keys, Count);

	if (Count > 0)
		free(Keys);

	log_exit("void");
	return;
}


int settings_value_count(const char *Key, size_t *Count)
{
		int ret = 0;
		const SETTINGS_KEY_ENTRY *ke = NULL;
		log_enter("Kewy=%s", Key, Count);
		
		ke = _get_key_entry(&_root, Key);
		if (ke != NULL)
			*Count = ke->ValueCount;
		else ret = ENOENT;
		
		log_exit("%i, *Count=%zu", ret, *Count);
		return ret;
}


int settings_values_enum(const char* Key, char*** Values, size_t* Count)
{
	int ret = 0;
	size_t tmpCount = 0;
	char** tmpValues = NULL;
	const SETTINGS_KEY_ENTRY* ke = NULL;
	log_enter("Key=\"%s\"; Values=0x%p; Count=0x%p", Key, Values, Count);

	ke = _get_key_entry(&_root, Key);
	if (ke != NULL) {
		tmpCount = ke->ValueCount;
		if (tmpCount > 0) {
			tmpValues = calloc(tmpCount, sizeof(char*));
			if (tmpValues != NULL)
				memcpy(tmpValues, ke->Values, ke->ValueCount * sizeof(char*));
			else ret = ENOMEM;
		}
	} else ret = ENOENT;

	if (ret == 0) {
		*Values = tmpValues;
		*Count = tmpCount;
	}

	log_exit("%i, *Values=0x%p, *Count=%zu", ret, *Values, *Count);
	return ret;
}


int settings_value_add(const char* Key, const char* Value)
{
	int ret = 0;
	PSETTINGS_KEY_ENTRY ke = NULL;
	char** tmpValues = NULL;
	log_enter("Key=\"%s\"; Value=\"%s\"", Key, Value);

	ke = _get_key_entry(&_root, Key);
	if (ke == NULL) {
		ret = settings_key_add(Key);
		if (ret == 0) {
			ke = _get_key_entry(&_root, Key);
			assert(ke != NULL);
		}
	}

	if (ret == 0) {
		tmpValues = realloc(ke->Values, (ke->ValueCount + 1)*sizeof(char *));
		if (tmpValues != NULL) {
			ke->Values = tmpValues;
			tmpValues[ke->ValueCount] = strdup(Value);
			if (tmpValues[ke->ValueCount] != NULL)
				++ke->ValueCount;
			else ret = ENOMEM;
		} else ret = ENOMEM;
	}

	log_exit("%i", ret);
	return ret;
}


int settings_value_get_string(const char* Key, size_t Index, char** Value, const char *Default)
{
	int ret = 0;
	const SETTINGS_KEY_ENTRY* ke = NULL;
	log_enter("Key=\"%s\"; Index=%zu; Value=0x%p; Default=\"%s\"", Key, Index, Value, Default);

	ke = _get_key_entry(&_root, Key);
	if (ke != NULL) {
		if (Index < ke->ValueCount)
			*Value = ke->Values[Index];
		else ret = ERANGE;
	} else {
		ret = ENOENT;
		if (Default != NULL) {
			*Value = (char *)Default;
			ret = 0;
		}
	}

	log_exit("%i, *Value=\"%s\"", ret, *Value);
	return ret;
}


int settings_value_get_int(const char* Key, size_t Index, int* Value, int Default)
{
	int ret = 0;
	char* r = NULL;
	char* tmp = NULL;
	long val = 0;
	char defValStr[100];
	log_enter("Key=\"%s\"; Index=%zu; Value=0x%p; Default=%i", Key, Index, Value, Default);

	snprintf(defValStr, sizeof(defValStr) / sizeof(defValStr[0]), "%i", Default);
	ret = settings_value_get_string(Key, Index, &r, defValStr);
	if (ret == 0) {
		val = strtol(r, &tmp, 0);
		if (tmp == r || (errno == ERANGE && (val == LONG_MIN || val == LONG_MAX)))
			ret = EINVAL;

		if (ret == 0)
			*Value = (int)val;
	}

	log_exit("%i, *Value=%i", ret, *Value);
	return ret;
}


int settings_value_set_string(const char* Key, size_t Index, const char* Value)
{
	int ret = 0;
	char* tmp = NULL;
	PSETTINGS_KEY_ENTRY ke = NULL;
	log_enter("Key=\"%s\"; Index=%zu; Value=\"%s\"", Key, Index, Value);

	ke = _get_key_entry(&_root, Key);
	if (ke != NULL) {
		if (ke->ValueCount > Index) {
			tmp = strdup(Value);
			if (tmp != NULL) {
				free(ke->Values[Index]);
				ke->Values[Index] = tmp;
			} else ret = ENOMEM;
		} else ret = settings_value_add(Key, Value);
	} else ret = settings_value_add(Key, Value);

	log_exit("%i", ret);
	return ret;
}


int settings_value_set_int(const char* Key, size_t Index, int Value)
{
	int ret = 0;
	char intStr[100];
	log_enter("Key=\"%s\"; Index=%zu; Value=%i", Key, Index, Value);

	snprintf(intStr, sizeof(intStr) / sizeof(intStr[0]), "%i", Value);
	ret = settings_value_set_string(Key, Index, intStr);

	log_exit("%i", ret);
	return ret;
}


int settings_value_delete(const char* Key, size_t Index)
{
	int ret = 0;
	PSETTINGS_KEY_ENTRY ke = NULL;
	log_enter("Key=\"%s\"; Index=%zu", Key, Index);

	ke = _get_key_entry(&_root, Key);
	if (ke != NULL) {
		if (ke->ValueCount > Index) {
			memmove(ke->Values + Index, ke->Values + Index + 1, (ke->ValueCount - Index - 1)*sizeof(char *));;
			--ke->ValueCount;
		} else ret = ERANGE;
	} else ret = ENOENT;

	log_exit("%i", ret);
	return ret;
}


void settings_values_free(char** Values, size_t Count)
{
	log_enter("Values=0x%p; Count=%zu", Values, Count);

	if (Count > 0)
		free(Values);

	log_exit("void");
	return;
}


int settings_load(const char* FileName, char Delimiter, char Comment)
{
	int ret = 0;
	FILE *f = NULL;
	size_t chunkSize = 1024;
	size_t bufSize = 0;
	char *buf = NULL;
	char *tmp = NULL;
	char *lineStart = NULL;
	char *delimiter = NULL;
	char *keyEnd = NULL;
	char *valueEnd = NULL;
	log_enter("FileName=\"%s\"; Delimiter=%c; Comment=%c", FileName, Delimiter, Comment);
	
	f = fopen(FileName, "r");
	if (f != NULL) {
		while (ret == 0 && !feof(f)) {
			buf = realloc(buf, bufSize + chunkSize + 1);
			if (buf == NULL) {
				ret = ENOMEM;
				continue;
			}
			
			memset(buf + bufSize, 0, chunkSize + 1);
			chunkSize = fread(buf + bufSize, 1, chunkSize, f);
			if (chunkSize == (size_t)-1 || ferror(f)) {
				ret = errno;
				continue;
			}
			
			bufSize += chunkSize;
		}

		if (ret == 0) {
			lineStart = buf;
			tmp = buf;
			log_info("%u bytes read", bufSize);
			for (size_t i = 0; i < bufSize; ++i) {
				if (*tmp == '\r' || *tmp == '\n') {
					*tmp = 0;
					if (*lineStart == Comment) {
						++tmp;
						continue;
					}
					
					delimiter = strchr(lineStart, Delimiter);
					if (delimiter != NULL && *lineStart != 0) {
						*delimiter = 0;
						keyEnd = delimiter - 1;
						while (keyEnd != lineStart && isspace(*keyEnd)) {
							*keyEnd = 0;
							--keyEnd;
						}
						
						while (isspace(*lineStart) && lineStart != keyEnd)
							++lineStart;
							
						++delimiter;
						while (isspace(*delimiter))
							++delimiter;
						
						valueEnd = tmp - 1;
						while (valueEnd != delimiter && isspace(*valueEnd)) {
							*valueEnd = 0;
							--valueEnd;
						}
						
						ret = settings_key_add(lineStart);
						if (ret == EEXIST)
							ret = 0;
							
						if (ret == 0)
							ret = settings_value_add(lineStart, delimiter);
					} else if (delimiter == NULL && *lineStart == 0)
						ret = EINVAL;
				
					lineStart = tmp + 1;
				}
				
				if (ret != 0)
					break;
				
				++tmp;
			}
		}
		
		free(buf);
		fclose(f);
		if (ret != 0)
			settings_free();
	} else ret = errno;
	
	log_exit("%i", ret);
	return ret;
}


int settings_save(const char* FileName, char Delimiter)
{
	int ret = 0;
	FILE *f = NULL;
	const SETTINGS_KEY_ENTRY *ke = NULL;
	char line[512];
	log_enter("FileName=\"%s\"; Delimiter=\"%c\"", FileName, Delimiter);

	if (FileName != NULL) {
		f = fopen(FileName, "w");
		if (f != NULL) {
			ke = _root.Keys;
			for (size_t i = 0; i < _root.KeyCount; ++i) {
				for (size_t j = 0; j < ke->ValueCount; ++j) {
					snprintf(line, sizeof(line) / sizeof(line[0]), "%s%c %s\n", ke->Name, Delimiter, ke->Values[j]);
					if (fwrite(line, strlen(line), 1, f) != strlen(line)) {
						ret = errno;
						break;
					}
				}

				if (ret != 0)
					break;

				++ke;
			}

			fclose(f);
			if (ret != 0)
				unlink(FileName);
		} else ret = errno;
	} else ret = EINVAL;

	log_exit("%i", ret);
	return ret;
}


void settings_free(void)
{
	PSETTINGS_KEY_ENTRY ke = NULL;
	log_enter("");

	ke = _root.Keys;
	for (size_t i = 0; i < _root.KeyCount; ++i) {
		for (size_t j = 0; j < ke->ValueCount; ++j)
			free(ke->Values[j]);
		
		free(ke->Values);
		free(ke->Name);
		++ke;
	}

	free(_root.Keys);
	_root.Keys = NULL;
	_root.KeyCount = 0;

	log_exit("void");
	return;
}


void settings_print(FILE *Stream)
{
	const SETTINGS_KEY_ENTRY* ke = NULL;
	log_enter("Stream=0x%p", Stream);

	ke = _root.Keys;
	for (size_t i = 0; i < _root.KeyCount; ++i) {
		if (ke->ValueCount > 1) {
			fprintf(Stream, "%s\n", ke->Name);
			for (size_t j = 0; j < ke->ValueCount; ++j)
				fprintf(Stream, "\t%s\n", ke->Values[j]);
		} else fprintf(Stream, "%s\t%s\n", ke->Name, ke->Values[0]);

		++ke;
	}

	log_exit("void");
	return;
}
