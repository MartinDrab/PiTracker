
#pragma once



typedef enum _ELogType {
	ltError,
	ltWarning,
	ltTrace,
	ltInfo,
} ELogType, * PELogType;


extern unsigned long _verbose;


#define log_enter(aFormat, ...)	\
	LogMessage(ltTrace, "[TRACE]: %s(" aFormat ")\n", __FUNCTION__, __VA_ARGS__ + 0)

#define log_exit(aFormat, ...)	\
	LogMessage(ltTrace, "[TRACE]: %s(-):" aFormat "\n", __FUNCTION__, __VA_ARGS__ + 0)

#define log_error(aFormat, ...)	\
	LogMessage(ltError, "[ERROR]: " aFormat "\n", __VA_ARGS__ + 0)

#define log_warning(aFormat, ...)	\
	LogMessage(ltWarning, "[WARNING]: " aFormat "\n", __VA_ARGS__ + 0)

#define log_trace(aFormat, ...)	\
	LogMessage(ltTrace, "[TRACE]: " aFormat "\n", __VA_ARGS__ + 0)

#define log_info(aFormat, ...)	\
	LogMessage(ltInfo, "[INFO]: " aFormat "\n", __VA_ARGS__ + 0)


void LogMessage(ELogType Type, const char* Format, ...);
