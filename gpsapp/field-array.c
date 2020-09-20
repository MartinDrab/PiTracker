
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "logging.h"
#include "field-array.h"


#ifndef min
#define min(a, b)	(a > b ? b : a)
#endif


int field_array_get(const char* Line, char Delimiter, char*** Array, size_t* Count)
{
	int ret = 0;
	const char* tmp = NULL;
	char** tmpArray = NULL;
	char** tmpArray2 = NULL;
	size_t tmpCount = 0;
	char buf[1024];
	size_t bufLen = 0;
	log_enter("Line=\"%s\"; Delimiter'%c'; Array=0x%p; Count=0x%p", Line, Delimiter, Array, Count);

	tmp = Line;
	while (ret == 0 && *tmp != '\0') {
		int quotes = 0;

		memset(buf, 0, sizeof(buf));
		bufLen = 0;
		while ((quotes || *tmp != Delimiter) && *tmp != '\0') {
			if (*tmp == '"')
				quotes = !quotes;

			buf[bufLen] = *tmp;
			++bufLen;
			++tmp;
		}

		if (*tmp != '\0')
			++tmp;

		if (bufLen >= 2 && buf[0] == '"' && buf[bufLen - 1] == '"') {
			memmove(buf, buf + 1, (bufLen - 2) * sizeof(char));
			bufLen -= 2;
			buf[bufLen] = '\0';
		}

		tmpArray2 = realloc(tmpArray, (tmpCount + 1) * sizeof(char*));
		if (tmpArray2 == NULL) {
			ret = ENOMEM;
			continue;
		}

		tmpArray = tmpArray2;
		tmpArray[tmpCount] = strdup(buf);
		if (tmpArray[tmpCount] == NULL) {
			ret = ENOMEM;
			continue;
		}

		log_trace("Array item: %s", tmpArray[tmpCount]);
		++tmpCount;
	}

	if (ret == 0) {
		*Array = tmpArray;
		*Count = tmpCount;
	}

	if (ret != 0) {
		for (size_t i = 0; i < tmpCount; ++i)
			free(tmpArray[i]);

		free(tmpArray);
	}

	log_exit("%i, *Array=0x%p, *Count=%zu", ret, *Array, *Count);
	return ret;
}


int field_array_extract(char** Array, const size_t Count, const STRING_FIELD_FORMAT* Formats, const size_t FormatCount)
{
	int ret = 0;

	for (size_t i = 0; i < min(Count, FormatCount); ++i) {
		switch (Formats->FieldType) {
		case sftNone:
			break;
		case sftString:
			*Formats->Target.String = strdup(*Array);
			if (*Formats->Target.String == NULL)
				ret = ENOMEM;
			break;
		case sftInt:
			*Formats->Target.Int = atoi(*Array);
			break;
		case sftLong:
			*Formats->Target.Long = strtol(*Array, NULL, 0);
			break;
		case sftUnsingedLong:
			*Formats->Target.ULong = strtoul(*Array, NULL, 0);
			break;
		case sftFloat:
			*Formats->Target.Float = strtof(*Array, NULL);
			break;
		case sftDouble:
			*Formats->Target.Double = strtod(*Array, NULL);
			break;
		}

		if (ret != 0) {
			for (size_t j = 0; j < i; ++j) {
				--Formats;
				if (Formats->FieldType == sftString)
					free(*Formats->Target.String);
			}

			break;
		}

		++Formats;
		++Array;
	}

	return ret;
}


void field_array_free(char** Array, size_t Count)
{
	for (size_t i = 0; i < Count; ++i)
		free(Array[i]);

	free(Array);

	return;
}
