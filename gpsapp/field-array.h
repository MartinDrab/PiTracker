
#pragma once


typedef enum _EStringFieldType {
	sftNone,
	sftString,
	sftInt,
	sftLong,
	sftUnsingedLong,
	sftFloat,
	sftDouble,
} EStringFieldType, * PEStringFieldType;

typedef struct _STRING_FIELD_FORMAT {
	EStringFieldType FieldType;
	union {
		void* Void;
		char** String;
		int* Int;
		long* Long;
		unsigned long* ULong;
		float* Float;
		double* Double;
	} Target;
	int Required;
} STRING_FIELD_FORMAT, * PSTRING_FIELD_FORMAT;


int field_array_get(const char* Line, char Delimiter, char*** Array, size_t* Count);
int field_array_extract(char** Array, const size_t Count, const STRING_FIELD_FORMAT* Formats, const size_t FormatCount);
void field_array_free(char** Array, size_t Count);