
#pragma once



typedef struct _SMS_MESSAGE {
	int Index;
	char *PhoneNumber;
	char* Name;
	char *Storage;
	char *Timestamp;
	char *Text;
	int StorageNo;
} SMS_MESSAGE, * PSMS_MESSAGE;

typedef struct _COMMAND_RESPONSE {
	char *Response;
	size_t ResponseSize;
	char **Lines;
	size_t LineCount;
} COMMAND_RESPONSE, *PCOMMAND_RESPONSE;

typedef enum _ESMSDeleteType {
	smsdtNormal,
	smsdtRecvRead,
	smsdtRecvReadSent,
	smsdtRecvReadSentUnsent,
	smsdtAll,
} ESMSDeleteType, *PESMSDeleteType;

typedef struct _GPS_RECORD {
	int GNSSStatus;
	int FixStatus;
	char* Timestamp;
	double Lattitude;
	double Longitude;
	double MSLAltitude;
	double Speed;
	double Orientation;
	int FixMode;
	int Reserved1;
	double HDOP;
	double PDOP;
	double VDOP;
	int Reserved2;
	int GNSSSatelitesInView;
	int GNSSSatelitesUsed;
	int GLONASSSatelitesUsed;
	int Reserved3;
	int CNoMax;
	int HPA;
	int VPA;
} GPS_RECORD, *PGPS_RECORD;


int command_pin_required(int SerialFD, int* Result);
int command_pin_enter(int SerialFD, const char* PIN);
int command_set_text_mode(int SerialFD, int Mode);
int command_sms_read(int SerialFD, int Index, PSMS_MESSAGE Message);
int command_sms_list(int SerialFD, const char* Type, SMS_MESSAGE **Messages, size_t *Count);
void sms_free(PSMS_MESSAGE Message);
void sms_array_free(PSMS_MESSAGE Messages, size_t Count);
int command_sms_delete(int SerialFD, int Index, ESMSDeleteType Type);
int command_sms_send(int SerialFD, const char *Phone, const char *Text);
int command_gnss_enable(int SerialFD, int Enable);
int command_gnss_info(int SerialFD, PGPS_RECORD Record);
void command_gnss_info_free(PGPS_RECORD Record);
int command_gnss_status(int SerialFD, int *Status);
int command_signal_quality(int SerialFD, int* Percentage, int* Second);
int command_battery(int SerialFD, int *Unknown, int *Percentage, int *Voltage);
int command_apn_set(int SerialFD, const char* Protocol, const char* URL, const char* UserName, const char* Password);
int command_gprs_connect(int SerialFD, int Connect);
int command_gprs_connected(int SerialFD, int* Connected);
int command_tcp_send(int SerialFD, const char* IP, int Port, const char* Data);
