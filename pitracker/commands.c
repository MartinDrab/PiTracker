
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "logging.h"
#include "serial.h"
#include "field-array.h"
#include "commands.h"



static int _is_hex_string(const char *Str)
{
	int ret = 1;
	size_t len = 0;
	log_enter("Str=\"%s\"", Str);

	while (*Str != '\0') {
		ret = (
			(*Str >= '0' && *Str <= '9') ||
			(*Str >= 'a' && *Str <= 'f') ||
			(*Str >= 'A' && *Str <= 'F'));
		if (!ret)
			break;

		++Str;
		++len;
	}

	if (ret)
		ret = ((len % 2) == 0);

	log_exit("%i", ret);
	return ret;
}

typedef enum _ESMSCommandType {
	sctList,
	sctOne,
} ESMSCommandType, *PESMSCommandType;

static int _process_sms(ESMSCommandType Type, const char *Command, char **Lines, size_t LineCount, SMS_MESSAGE **Messages, size_t *Count)
{
	int ret = 0;
	size_t len = 0;
	size_t cmdLen = 0;
	const char *line = NULL;
	SMS_MESSAGE msg;
	PSMS_MESSAGE tmpMessages = NULL;
	PSMS_MESSAGE tmpMessages2 = NULL;
	size_t tmpCount = 0;
	log_enter("Type=%u; Command=\"%s\"; Lines=0x%p; LineCount=%zu; Messages=0x%p; Count=0x%p", Type, Command, Lines, LineCount, Messages, Count);

	cmdLen = strlen(Command);
	while (ret == 0 && LineCount > 0) {
		line = *Lines;
		len = strlen(line);
		if (len > cmdLen && memcmp(line, Command, cmdLen * sizeof(char)) == 0) {
			char** arr = NULL;
			size_t arrSize = 0;
			
			memset(&msg, 0, sizeof(msg));
			ret = field_array_get(line + cmdLen, ',', &arr, &arrSize);
			if (ret == 0) {
				STRING_FIELD_FORMAT fields[] = {
					{sftInt, {&msg.Index}},
					{sftString, {&msg.Storage}},
					{sftString, {&msg.PhoneNumber}},
					{sftString, {&msg.Name}},
					{sftString, {&msg.Timestamp}},
				};
				size_t bias = 0;

				if (Type == sctOne)
					bias = 1;

				ret = field_array_extract(arr, arrSize, fields + bias, sizeof(fields) / sizeof(fields[0]) - bias);
				field_array_free(arr, arrSize);
			}

			++Lines;
			--LineCount;
			if (ret == 0 && LineCount > 0) {
				line = *Lines;
				msg.Text = strdup(line);
				if (msg.Text == NULL)
					ret = ENOMEM;

				if (ret == 0) {
					len = strlen(msg.Text);
					if (_is_hex_string(msg.Text)) {
						for (size_t i = 0; i < len / 2; ++i) {
							char d1 = msg.Text[i * 2];
							char d2 = msg.Text[i * 2 + 1];

							d1 &= (char)~(0x60);
							d1 = (d1 & 0x10) ? (d1 & 0xf) : (d1 + 9);
							d2 &= (char)~(0x60);
							d2 = (d2 & 0x10) ? (d2 & 0xf) : (d2 + 9);
							msg.Text[i] = (char)((d1 << 4) + d2);
						}

						msg.Text[len / 2] = '\0';
					}

					tmpMessages2 = realloc(tmpMessages, (tmpCount + 1)*sizeof(SMS_MESSAGE));
					if (tmpMessages2 == NULL) {
						sms_free(&msg);
						ret = ENOMEM;
					}

					if (ret == 0) {
						tmpMessages = tmpMessages2;
						tmpMessages[tmpCount] = msg;
						++tmpCount;
					}
				}
			}
		}

		if (LineCount > 0) {
			++Lines;
			--LineCount;
		}
	}

	if (ret == 0) {
		*Messages = tmpMessages;
		*Count = tmpCount;
	}

	if (ret != 0) {
		for (size_t i = 0; i < tmpCount; ++i)
			sms_free(tmpMessages + i);

		free(tmpMessages);
	}

	log_exit("%i, *Messages=0x%p, *Count=%zu", ret, *Messages, *Count);
	return ret;
}


static int _standard_command_issue(int SerialFD, const char *Command, PCOMMAND_RESPONSE Response)
{
	int ret = 0;
	size_t rc = 0;
	char* r = NULL;
	char** ls = NULL;
	size_t lc = 0;
	ESerialCommandStatus status = scsUnknown;
	log_enter("SerialFD=%i; Command=\"%s\"; Response=0x%p", SerialFD, Command, Response);

	ret = serial_command_with_response(SerialFD, Command, 1, 1, 1, &r, &rc);
	if (ret == 0) {
		ret = serial_response_to_lines(r, rc, &ls, &lc);
		if (ret == 0) {
			status = serial_command_status(ls, lc);
			if (status != scsOK)
				ret = -1;
		
			if (ret == 0) {
				Response->LineCount = lc;
				Response->Lines = ls;
				Response->Response = r;
				Response->ResponseSize = rc;
			}

			if (ret != 0)
				free(ls);
		}

		if (ret != 0)
			free(r);
	}

	log_exit("%i", ret);
	return ret;
}


static int _standard_command_issue_ex(int SerialFD, const char *Command, int CR, int LF, int OKSearch, PCOMMAND_RESPONSE Response)
{
	int ret = 0;
	size_t rc = 0;
	char* r = NULL;
	char** ls = NULL;
	size_t lc = 0;
	ESerialCommandStatus status = scsUnknown;
	log_enter("SerialFD=%i; Command=\"%s\"; CR=%u; LF=%u; OKSearch=%u; Response=0x%p", SerialFD, Command, CR, LF, OKSearch, Response);

	ret = serial_command_with_response(SerialFD, Command, CR, LF, OKSearch, &r, &rc);
	if (ret == 0) {
		ret = serial_response_to_lines(r, rc, &ls, &lc);
		if (ret == 0) {
			status = serial_command_status(ls, lc);
			if (status != scsOK)
				ret = -1;

			if (ret == 0) {
				Response->LineCount = lc;
				Response->Lines = ls;
				Response->Response = r;
				Response->ResponseSize = rc;
			}

			if (ret != 0)
				free(ls);
		}

		if (ret != 0)
			free(r);
	}

	log_exit("%i", ret);
	return ret;
}


static char *_standard_command_getline(const COMMAND_RESPONSE* Response, const char* Start)
{
	char *l = NULL;
	char *ret = NULL;
	size_t respLen = 0;
	log_enter("Response=0x%p; Start=\"%s\"", Response, Start);

	respLen = strlen(Start);
	for (size_t i = 0; i < Response->LineCount; ++i) {
		l = Response->Lines[i];
		if (strlen(l) >= respLen && memcmp(l, Start, respLen * sizeof(char)) == 0) {
			ret = l + respLen;
			break;
		}
	}

	log_exit("\"%s\"", ret);
	return ret;
}

static void _standard_command_free(PCOMMAND_RESPONSE Response)
{
	log_enter("Response=0x%p", Response);

	free(Response->Lines);
	free(Response->Response);

	log_exit("void");
	return;
}


int command_pin_required(int SerialFD, int* Result)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Result=0x%p", SerialFD, Result);

	ret = _standard_command_issue(SerialFD, "AT+CPIN?", &r);
	if (ret == 0) {
		*Result = serial_command_contains(r.Lines, r.LineCount, "+CPIN: SIM PIN");
		_standard_command_free(&r);
	}

	log_exit("%i, *Result=%i", ret, *Result);
	return ret;
}


int command_pin_enter(int SerialFD, const char* PIN)
{
	int ret = 0;
	char pinCommand[256];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; PIN=\"%s\"", SerialFD, PIN);

	snprintf(pinCommand, sizeof(pinCommand) / sizeof(pinCommand[0]), "AT+CPIN=%s", PIN);
	ret = _standard_command_issue(SerialFD, pinCommand, &r);
	if (ret == 0)
		_standard_command_free(&r);

	log_exit("%i", ret);
	return ret;
}


int command_set_text_mode(int SerialFD, int Mode)
{
	int ret = 0;
	char cmd[256];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Mode=%i", SerialFD, Mode);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CMGF=%i", Mode);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0)
		_standard_command_free(&r);

	log_exit("%i", ret);
	return ret;
}


int command_sms_read(int SerialFD, int Index, PSMS_MESSAGE Message)
{
	int ret = 0;
	char cmd[256];
	COMMAND_RESPONSE r;
	PSMS_MESSAGE msgs;
	size_t msgCount = 0;
	log_enter("SerialFD=%u; Index=%i; Message=0x%p", SerialFD, Index, Message);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CMGR=%i", Index);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0) {
		ret = _process_sms(sctOne, "+CMGR: ", r.Lines, r.LineCount, &msgs, &msgCount);
		if (ret == 0) {
			if (msgCount > 0) {
				assert(msgCount == 1);
				*Message = msgs[0];
			} else ret = ENOENT;

			free(msgs);
		}

		_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}


int command_sms_list(int SerialFD, const char* Type, SMS_MESSAGE **Messages, size_t *Count)
{
	int ret = 0;
	char cmd[256];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%u; Type=\"%s\"; Messages=0x%p; Count=0x%p", SerialFD, Type, Messages, Count);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CMGL=\"%s\"", Type);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0) {
		ret = _process_sms(sctList, "+CMGL: ", r.Lines, r.LineCount, Messages, Count);
		_standard_command_free(&r);
	}

	log_exit("%i, *Messages=0x%p, *Count=%zu", ret, *Messages, *Count);
	return ret;
}


void sms_free(PSMS_MESSAGE Message)
{
	log_enter("Message=0x%p", Message);

	free(Message->PhoneNumber);
	free(Message->Name);
	free(Message->Storage);
	free(Message->Text);
	free(Message->Timestamp);

	log_exit("void");
	return;
}


void sms_array_free(PSMS_MESSAGE Messages, size_t Count)
{
	for (size_t i = 0; i < Count; ++i)
		sms_free(Messages + i);

	free(Messages);

	log_exit("void");
	return;
}


int command_sms_delete(int SerialFD, int Index, ESMSDeleteType Type)
{
	int ret = 0;
	char cmd[256];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Index=%i; Type=%u", SerialFD, Index, Type);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CMGD=%i,%u", Index, Type);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0)
		_standard_command_free(&r);

	log_exit("%i", ret);
	return ret;
}


int command_sms_send(int SerialFD, const char *Phone, const char *Text)
{
	int ret = 0;
	char cmd[1024];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Phone=\"%s\"; Text=\"%s\"", SerialFD, Phone, Text);

	memset(&r, 0, sizeof(r));
	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CMGS=\"%s\"\n", Phone);
	ret = serial_command_with_response(SerialFD, cmd, 1, 0, 0, &r.Response, &r.ResponseSize);
	if (ret == 0) {
		ret = serial_response_to_lines(r.Response, r.ResponseSize, &r.Lines, &r.LineCount);
		if (ret == 0) {
			if (!serial_command_contains(r.Lines, r.LineCount, "> "))
				ret = -1;

			free(r.Lines);
		}

		free(r.Response);
	}

	if (ret == 0) {
		snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "%s\x1a", Text);
		ret = _standard_command_issue_ex(SerialFD, cmd, 0, 0, 1, &r);
		if (ret == 0)
			_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}


int command_gnss_enable(int SerialFD, int Enable)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	char cmd[256];

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CGNSPWR=%u", Enable);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0)
		_standard_command_free(&r);

	return ret;
}


int command_gnss_status(int SerialFD, int* Status)
{
	int ret = 0;
	char* l = NULL;
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Status=0x%p", SerialFD, Status);

	ret = _standard_command_issue(SerialFD, "AT+CGNSPWR?", &r);
	if (ret == 0) {
		ret = ENOENT;
		l = _standard_command_getline(&r, "+CGNSPWR: ");
		if (l != NULL) {
			*Status = (int)strtol(l, NULL, 0);
			ret = 0;
		}

		_standard_command_free(&r);
	}

	log_exit("%k, *Status=%i", ret, *Status);
	return ret;
}


int command_gnss_info(int SerialFD, PGPS_RECORD Record)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	const char *l = NULL;
	char **arr = NULL;
	size_t arrSize = 0;
	log_enter("SerialFD=%i; Record=0x%p", SerialFD, Record);

	ret = _standard_command_issue(SerialFD, "AT+CGNSINF", &r);
	if (ret == 0) {
		ret = ENOENT;
		l = _standard_command_getline(&r, "+CGNSINF: ");
		if (l != NULL) {
			ret = field_array_get(l, ',', &arr, &arrSize);
			if (ret == 0) {
				STRING_FIELD_FORMAT formats[] = {
					{sftInt, {&Record->GNSSStatus}},
					{sftInt, {&Record->FixStatus}},
					{sftString, {&Record->Timestamp}},
					{sftDouble, {&Record->Lattitude}},
					{sftDouble, {&Record->Longitude}},
					{sftDouble, {&Record->MSLAltitude}},
					{sftDouble, {&Record->Speed}},
					{sftDouble, {&Record->Orientation}},
					{sftNone, {NULL}},
					{sftNone, {NULL}},
					{sftDouble, {&Record->HDOP}},
					{sftDouble, {&Record->PDOP}},
					{sftDouble, {&Record->VDOP}},
					{sftNone, {NULL}},
					{sftInt, {&Record->GNSSSatelitesInView}},
					{sftInt, {&Record->GNSSSatelitesUsed}},
					{sftInt, {&Record->GLONASSSatelitesUsed}},
					{sftNone, {NULL}},
					{sftInt, {&Record->CNoMax}},
					{sftInt, {&Record->HPA}},
					{sftInt, {&Record->VPA}},
				};

				memset(Record, 0, sizeof(GPS_RECORD));
				ret = field_array_extract(arr, arrSize, formats, sizeof(formats) / sizeof(formats[0]));
				field_array_free(arr, arrSize);
			}
		}

		_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}


void command_gnss_info_free(PGPS_RECORD Record)
{
	log_enter("Record=0x%p", Record);

	free(Record->Timestamp);

	log_exit("void");
	return;
}


int command_signal_quality(int SerialFD, int *Percentage, int *Second)
{
	int ret = 0;
	char* l = NULL;
	char** arr = NULL;
	size_t arrSize = 0;
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Percentage=0x%p; Second=0x%p", SerialFD, Percentage, Second);

	ret = _standard_command_issue(SerialFD, "AT+CSQ", &r);
	if (ret == 0) {
		ret = ENOENT;
		l = _standard_command_getline(&r, "+CSQ: ");
		if (l != NULL) {
			ret = field_array_get(l, ',', &arr, &arrSize);
			if (ret == 0) {
				int quality = 0;
				int second = 0;

				STRING_FIELD_FORMAT formats[] = {
					{sftInt, {&quality}},
					{sftInt, {&second}},
				};

				ret = field_array_extract(arr, arrSize, formats, sizeof(formats) / sizeof(formats[0]));
				if (ret == 0) {
					*Percentage = (quality * 827 + 127) >> 8;
					if (Second != NULL)
						*Second = second;
				}

				field_array_free(arr, arrSize);
			}
		}

		_standard_command_free(&r);
	}

	log_exit("%i, *Percentage=%i", ret, *Percentage);
	return ret;
}


int command_battery(int SerialFD, int* Unknown, int* Percentage, int* Voltage)
{
	int ret = 0;
	char* l = NULL;
	char** arr = NULL;
	size_t arrSize = 0;
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; Unknown=0x%p; Percentage=0x%p; Voltage=0x%p", SerialFD, Unknown, Percentage, Voltage);

	ret = _standard_command_issue(SerialFD, "AT+CBC", &r);
	if (ret == 0) {
		ret = ENOENT;
		l = _standard_command_getline(&r, "+CBC: ");
		if (l != NULL) {
			ret = field_array_get(l, ',', &arr, &arrSize);
			if (ret == 0) {
				int unk = 0;
				int percents = 0;
				int voltage = 0;

				STRING_FIELD_FORMAT formats[] = {
					{sftInt, {&unk}},
					{sftInt, {&percents}},
					{sftInt, {&voltage}},
				};
				size_t bias = 0;

				if (arrSize == 2)
					bias = 1;

				ret = field_array_extract(arr, arrSize, formats + bias, sizeof(formats) / sizeof(formats[0]) - bias);
				if (ret == 0) {
					if (Unknown != NULL)
						*Unknown = unk;
					
					if (Percentage != NULL)
						*Percentage = percents;
					
					if (Voltage != NULL)
						*Voltage = voltage;
				}

				field_array_free(arr, arrSize);
			}
		}

		_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}


int command_apn_set(int SerialFD, const char* Protocol, const char* URL, const char* UserName, const char* Password)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	char cmd[256];
	log_enter("SerialFD=%i; Protocol=\"%s\"; URL=\"%s\"; UserName=\"%s\"; Password=\"%s\"", SerialFD, Protocol, URL, UserName, Password);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CSTT=\"%s\",\"%s\",\"%s\"", URL, UserName, Password);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0) {
		_standard_command_free(&r);
		snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CGDCONT=1,\"%s\",\"%s\"", Protocol, URL);
		ret = _standard_command_issue(SerialFD, cmd, &r);
		if (ret == 0)
			_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}


int command_gprs_connect(int SerialFD, int Connect)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	char cmd[256];
	log_enter("SerialFD=%i; Connect=%i", SerialFD, Connect);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CREG=%i", Connect);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0) {
		_standard_command_free(&r);
		snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CGACT=%i,1", Connect);
		ret = _standard_command_issue(SerialFD, cmd, &r);
		if (ret == 0) {
			_standard_command_free(&r);
			snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CGATT=%i", Connect);
			ret = _standard_command_issue(SerialFD, cmd, &r);
			if (ret == 0)
				_standard_command_free(&r);
		}
	}

	if (ret == 0 && Connect) {
		ret = _standard_command_issue(SerialFD, "AT+CIICR", &r);
		if (ret == 0)
			_standard_command_free(&r);

		ret = _standard_command_issue(SerialFD, "AT+CIFSR", &r);
		if (ret == 0)
			_standard_command_free(&r);

		ret = 0;
	}

	log_exit("%i", ret);
	return ret;
}


int command_gprs_connected(int SerialFD, int* Connected)
{
	int ret = 0;
	COMMAND_RESPONSE r;
	char* l = NULL;
	log_enter("SerialFD=%i; Connected=0x%p", SerialFD, Connected);

	ret = _standard_command_issue(SerialFD, "AT+CGATT?", &r);
	if (ret == 0) {
		l = _standard_command_getline(&r, "+CGATT: ");
		if (l != NULL) {
			switch (*l) {
				case '0':
					*Connected = 0;
					break;
				case '1':
					*Connected = 1;
					break;
				default:
					ret = EINVAL;
					break;
			}
		} else ret = ENOENT;

		_standard_command_free(&r);
	}

	log_exit("%i, *Connected=%i", ret, *Connected);
	return ret;
}


int command_tcp_send(int SerialFD, const char* IP, int Port, const char* Data)
{
	int ret = 0;
	char cmd[4096];
	COMMAND_RESPONSE r;
	log_enter("SerialFD=%i; IP=\"%s\"; Port=%i; Data=\"%s\"", SerialFD, IP, Port, Data);

	snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "AT+CIPSTART=\"TCP\",\"%s\",\"%i\"", IP, Port);
	ret = _standard_command_issue(SerialFD, cmd, &r);
	if (ret == 0) {
		_standard_command_free(&r);
		memset(&r, 0, sizeof(r));
		ret = serial_command_with_response(SerialFD, "AT+CIPSEND", 1, 0, 0, &r.Response, &r.ResponseSize);
		if (ret == 0) {
			ret = serial_response_to_lines(r.Response, r.ResponseSize, &r.Lines, &r.LineCount);
			if (ret == 0) {
				if (!serial_command_contains(r.Lines, r.LineCount, "> "))
					ret = -1;

				free(r.Lines);
			}

			free(r.Response);
		}

		if (ret == 0) {
			snprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), "%s\x1a", Data);
			ret = _standard_command_issue_ex(SerialFD, cmd, 0, 0, 1, &r);
			if (ret == 0)
				_standard_command_free(&r);
		}

		ret = _standard_command_issue(SerialFD, "AT+CIPCLOSE", &r);
		if (ret == 0)
			_standard_command_free(&r);

		ret = _standard_command_issue(SerialFD, "AT+CIPSHUT", &r);
		if (ret == 0)
			_standard_command_free(&r);
	}

	log_exit("%i", ret);
	return ret;
}
