
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
#include "serial.h"
#include "line-buffer.h"
#include "field-array.h"
#include "commands.h"
#include "settings.h"
#include "accounts.h"
#include "cmdline.h"


//  +CMTI: "SM",0, incomming SMS on index 0



static void* _notifyCallbackHandle = NULL;
static int _gpsPeriod = 0;
static int _syncPeriod = 0;


typedef enum _EControlCommand {
	eccLogin,
	eccLogout,
	eccUserDelete,
	eccStatus,
	eccGPSOn,
	eccGPSOff,
	eccMap,
	eccFence,
	eccVersion,
	eccSMS,
	eccChangePassword,
	eccReboot,
	eccGetOption,
	eccSetOption,
	eccAPN,
	eccGPRSOn,
	eccGPRSOff,
	eccGPRSSync,
} EControlCommand, *PEControlCommand;

typedef int (CONTROL_COMMAND_CALLBACK)(int SerialFD, const char *Phone, EControlCommand Type, char **Args, size_t ArgCount, int *SendResult);

#define CONTROL_FLAG_AUTH_REQUIRED			0x1
#define CONTROL_FLAG_ADMIN_REQUIRED			0x2
#define CONTROL_FLAG_SAVE_ACCOUNTS			0x4
#define CONTROL_FLAG_SAVE_SETTINGS			0x8

typedef struct _CONTROL_COMMAND {
	EControlCommand Type;
	const char* String;
	size_t ArgCount;
	CONTROL_COMMAND_CALLBACK* Callback;
	int Flags;
} CONTROL_COMMAND, *PCONTROL_COMMAND;


int status_sms_callback(int SerialFD, const char* Phone, EControlCommand Type, char** Args, size_t ArgCount, int* SendResult)
{
	int ret = 0;
	char msg[256];
	int loggedIn = 0;
	log_enter("SerialFD=%i; Phone=\"%s\"; Type=%u; Args=0x%p; ArgCount=%zu; SendResult=0x%p", SerialFD, Phone, Type, Args, ArgCount, SendResult);

	memset(msg, 0, sizeof(msg));
	ret = account_logged_in(Phone, &loggedIn);
	if (ret != 0)
		loggedIn = 0;

	switch (Type) {
		case eccStatus: {
			int gnssStatus = 0;
			int signalQuality = 0;
			int gprs = 0;
			GPS_RECORD gpsRecord;
			int batteryCharge = 0;

			ret = command_signal_quality(SerialFD, &signalQuality, NULL);
			if (ret != 0) {
				log_error("Unable to get GPRS signal: %i", ret);
				signalQuality = -1;
			}

			ret = command_gprs_connected(SerialFD, &gprs);
			if (ret != 0) {
				log_error("Unable to get GPRS status: %i", ret);
				gprs = -1;
			}

			ret = command_battery(SerialFD, NULL, &batteryCharge, NULL);
			if (ret != 0) {
				log_error("Unable to get battery charge: %i", ret);
				batteryCharge = -1;
			}

			ret = command_gnss_status(SerialFD, &gnssStatus);
			if (ret != 0) {
				log_error("Unable to get GNSS status: %i", ret);
				gnssStatus = -1;
			}

			snprintf(msg, sizeof(msg), "GSM %i %%; GPRS %i; BATTERY: %i %%; GPS: %i", signalQuality, gprs, batteryCharge, gnssStatus);
			if (gnssStatus == 1 && loggedIn) {
				ret = command_gnss_info(SerialFD, &gpsRecord);
				if (ret != 0)
					log_error("Unable to get GNSS location: %i", ret);

				if (ret == 0 && gpsRecord.FixStatus == 1) {
					snprintf(msg, sizeof(msg), "GSM; %i %%; GPRS %i; BATTERY %i %%; GPS: %i/%i; Time: %s; Lat: %lf; Long: %lf", signalQuality, gprs, batteryCharge, gpsRecord.GNSSSatelitesUsed, gpsRecord.GNSSSatelitesInView, gpsRecord.Timestamp, gpsRecord.Lattitude, gpsRecord.Longitude);
					command_gnss_info_free(&gpsRecord);
				}
			}

			ret = 0;
		} break;
		default:
			break;
	}

	if (ret == 0 && msg[0] != '\0') {
		ret = command_sms_send(SerialFD, Phone, msg);
		if (ret != 0)
			log_error("Unable to send response SMS: %i", ret);

		if (ret == 0)
			*SendResult = 0;
	}

	log_exit("%i, *SendResult=%i", ret, *SendResult);
	return ret;
}


int account_sms_callback(int SerialFD, const char* Phone, EControlCommand Type, char** Args, size_t ArgCount, int* SendResult)
{
	int ret = 0;
	char msg[256];
	log_enter("SerialFD=%i; Phone=\"%s\"; Type=%u; Args=0x%p; ArgCount=%zu; SendResult=0x%p", SerialFD, Phone, Type, Args, ArgCount, SendResult);

	memset(msg, 0, sizeof(msg));
	switch (Type) {
		case eccUserDelete:
			break;
		case eccLogin:
			ret = account_login(Phone, Args[0]);
			if (ret == ENOENT) {
				ret = account_login("*", Args[0]);
				if (ret == EEXIST)
					ret = 0;

				if (ret == 0)
					ret = account_add(Phone, Args[0], 1);
			
				if (ret == 0)
					ret = account_login(Phone, Args[0]);
			
				if (ret == 0) {
					ret = accounts_save();
					if (ret == 0)
						ret = settings_save(_configFile, ':');

					if (ret != 0)
						log_warning("Unable to save settings: %i", ret);
				}
			}

			switch (ret) {
				case EEXIST:
					ret = 0;
					strncpy(msg, "ALREADY_LOGGED_IN", sizeof(msg) / sizeof(msg[0]));
					break;
				case EACCES:
					ret = 0;
					strncpy(msg, "INVALID_PASSWORD", sizeof(msg) / sizeof(msg[0]));
					break;
				case ENOENT:
					ret = 0;
					strncpy(msg, "UNKNOWN_ACCOUNT", sizeof(msg) / sizeof(msg[0]));
					break;
				default:
					break;
			}
			break;
		case eccLogout:
			ret = account_logout(Phone);
			if (ret == EEXIST) {
				ret = 0;
				strncpy(msg, "NOT_LOGGED_IN", sizeof(msg) / sizeof(msg[0]));
			}
			break;
		case eccChangePassword:
			ret = account_set_password(Phone, Args[0], Args[1]);
			if (ret == 0) {
				ret = accounts_save();
				if (ret == 0)
					ret = settings_save(_configFile, ':');
			}
			break;
		default:
			break;
	}

	if (ret == 0 && msg[0] != '\0') {
		ret = command_sms_send(SerialFD, Phone, msg);
		if (ret != 0)
			log_error("Unable to send response SMS: %i", ret);

		if (ret == 0)
			*SendResult = 0;
	}

	log_exit("%i, *SendResult=%i", ret, *SendResult);
	return ret;
}


int gps_control_sms_callback(int SerialFD, const char *Phone, EControlCommand Type, char** Args, size_t ArgCount, int* SendResult)
{
	int ret = 0;
	char msg[256];
	int gnssStatus = 0;
	GPS_RECORD gpsRecord;
	log_enter("SerialFD=%i; Phone=\"%s\"; Type=%u; Args=0x%p; ArgCount=%zu; SendResult=0x%p", SerialFD, Phone, Type, Args, ArgCount, SendResult);

	memset(msg, 0, sizeof(msg));
	switch (Type) {
		case eccGPSOn:
			ret = command_gnss_enable(SerialFD, 1);
			if (ret == 0) {
				settings_value_set_int("gps", 0, 1);
				settings_save(_configFile, ':');
			}
			break;
		case eccGPSOff:
			ret = command_gnss_enable(SerialFD, 0);
			if (ret == 0) {
				settings_value_set_int("gps", 0, 0);
				settings_save(_configFile, ':');
			}
			break;
		case eccMap:
			ret = command_gnss_status(SerialFD, &gnssStatus);
			if (ret == 0) {
				if (!gnssStatus) {
					ret = command_gnss_enable(SerialFD, 1);
					if (ret != 0)
						log_error("Unable to enable GNSS: %i", ret);

					if (ret == 0)
						sleep(60);
				}

				if (ret == 0) {
					ret = command_gnss_info(SerialFD, &gpsRecord);
					if (ret != 0)
						log_error("Unable to get GNSS location: %i", ret);

					if (ret == 0 && gpsRecord.FixStatus == 1) {
						snprintf(msg, sizeof(msg) / sizeof(msg[0]), "https://mapy.cz/zakladni?x=%lf&y=%lf", gpsRecord.Longitude, gpsRecord.Lattitude);
						command_gnss_info_free(&gpsRecord);
					}

					if (!gnssStatus) {
						ret = command_gnss_enable(SerialFD, 0);
						if (ret != 0)
							log_error("Unable to disable GNSS: %i", ret);
					}
				}
			} else log_error("Unable to get GNSS status: %i", ret);
			break;
		default:
			ret = -1;
			break;
	}

	if (ret == 0 && msg[0] != '\0') {
		ret = command_sms_send(SerialFD, Phone, msg);
		if (ret != 0)
			log_error("Unable to send response SMS: %i", ret);

		if (ret == 0)
			*SendResult = 0;
	}

	log_exit("%i, *SendResult=%i", ret, *SendResult);
	return ret;
}


int gprs_control_sms_callback(int SerialFD, const char* Phone, EControlCommand Type, char** Args, size_t ArgCount, int* SendResult)
{
	int ret = 0;
	char msg[256];
	char* un = NULL;
	char* pass = NULL;
	log_enter("SerialFD=%i; Phone=\"%s\"; Type=%u; Args=0x%p; ArgCount=%zu; SendResult=0x%p", SerialFD, Phone, Type, Args, ArgCount, SendResult);

	memset(msg, 0, sizeof(msg));
	switch (Type) {
		case eccGPRSOn:
			ret = command_gprs_connect(SerialFD, 1);
			if (ret == 0) {
				settings_value_set_int("gprs", 0, 1);
				settings_save(_configFile, ':');
			}
			break;
		case eccGPRSOff:
			ret = command_gprs_connect(SerialFD, 0);
			if (ret == 0) {
				settings_value_set_int("gprs", 0, 0);
				settings_save(_configFile, ':');
			}
			break;
		case eccAPN:
			un = "";
			pass = "";
			if (ArgCount >= 3)
				un = Args[2];

			if (ArgCount >= 4)
				pass = Args[3];

			ret = command_apn_set(SerialFD, Args[0], Args[1], un, pass);
			break;
		default:
			ret = -1;
			break;
	}

	if (ret == 0 && msg[0] != '\0') {
		ret = command_sms_send(SerialFD, Phone, msg);
		if (ret != 0)
			log_error("Unable to send response SMS: %i", ret);

		if (ret == 0)
			*SendResult = 0;
	}

	log_exit("%i, *SendResult=%i", ret, *SendResult);
	return ret;
}


static CONTROL_COMMAND _ccs[] = {
	{eccLogin, "#login", 1, account_sms_callback, CONTROL_FLAG_SAVE_ACCOUNTS},
	{eccLogout, "#logout", 0, account_sms_callback, 0},
	{eccUserDelete, "#userdel", 1, account_sms_callback, CONTROL_FLAG_SAVE_ACCOUNTS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccStatus, "#status", 0, status_sms_callback, 0},
	{eccGPSOn, "#gpson", 0, gps_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccGPSOff, "#gpsoff", 0, gps_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccMap, "#map", 0, gps_control_sms_callback, CONTROL_FLAG_AUTH_REQUIRED},
	{eccFence, "#fence", 1, gps_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccVersion, "#ver", 0, status_sms_callback, 0},
	{eccSMS, "#sms", 2, NULL, CONTROL_FLAG_AUTH_REQUIRED},
	{eccChangePassword, "#pass", 2, account_sms_callback, CONTROL_FLAG_SAVE_ACCOUNTS | CONTROL_FLAG_AUTH_REQUIRED},
	{eccReboot, "#reboot", 0, NULL, CONTROL_FLAG_AUTH_REQUIRED},
	{eccGetOption, "#getopt", 1, NULL, CONTROL_FLAG_AUTH_REQUIRED},
	{eccSetOption, "#setopt", 2, NULL, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccAPN, "#apn", 2, gprs_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccGPRSOn, "#gprson", 0, gprs_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
	{eccGPRSOff, "#gprsoff", 0, gprs_control_sms_callback, CONTROL_FLAG_SAVE_SETTINGS  | CONTROL_FLAG_AUTH_REQUIRED},
};


const CONTROL_COMMAND* control_command(const char *Command)
{
	const CONTROL_COMMAND *tmp = NULL;
	const CONTROL_COMMAND *ret = NULL;
	log_enter("Command=\"%s\"", Command);

	tmp = _ccs;
	for (size_t i = 0; i < sizeof(_ccs) / sizeof(_ccs[0]); ++i) {
		if (strcmp(tmp->String, Command) == 0) {
			ret = tmp;
			break;
		}

		++tmp;
	}

	log_exit("0x%p", ret);
	return ret;
}


static int _sms_process(int SerialFD, const SMS_MESSAGE* Msg)
{
	int ret = 0;
	int loggedIn = 0;
	int sendResult = 0;
	char** arr = NULL;
	size_t arrSize = 0;
	const CONTROL_COMMAND* cc = NULL;
	log_enter("SerialFD=%i; Msg=0x%p", SerialFD, Msg);

	ret = field_array_get(Msg->Text, ' ', &arr, &arrSize);
	if (ret == 0) {
		if (arrSize == 0) {
			ret = -1;
			log_error("SHS contains no argument");
		}

		if (ret == 0) {
			cc = control_command(arr[0]);
			if (cc == NULL) {
				ret = -2;
				log_error("Unknown command \"%s\"", arr[0]);
			}
		}

		if (ret == 0) {
			if (cc->Callback != NULL) {
				if (cc->ArgCount <= arrSize - 1) {
					sendResult = 1;
					ret = account_logged_in(Msg->PhoneNumber, &loggedIn);
					if (ret == ENOENT) {
						loggedIn = 0;
						ret = 0;
					}

					if (ret == 0 && (cc->Flags & CONTROL_FLAG_AUTH_REQUIRED) != 0 && !loggedIn) {
						sendResult = 0;
						ret = command_sms_send(SerialFD, Msg->PhoneNumber, "NOT_AUTHENTICATED");
					}

					if (ret == 0)
						ret = cc->Callback(SerialFD, Msg->PhoneNumber, cc->Type, arr + 1, arrSize - 1, &sendResult);
				} else {
					sendResult = 0;
					ret = command_sms_send(SerialFD, Msg->PhoneNumber, "NOT_ENOUGH_ARGUMENTS");
				}
			} else {
				sendResult = 0;
				ret = command_sms_send(SerialFD, Msg->PhoneNumber, "NOT_IMPLEMENTED");
			}
		}

		if (sendResult) {
			char retMsg[60];

			memset(retMsg, 0, sizeof(retMsg));
			if (ret == 0)
				strncpy(retMsg, "OK", sizeof(retMsg));
			else snprintf(retMsg, sizeof(retMsg), "ERROR: %i", ret);
			
			ret = command_sms_send(SerialFD, Msg->PhoneNumber, retMsg);
			if (ret != 0)
				log_error("Unable to send response: %i", ret);
		}

		ret = command_sms_delete(SerialFD, Msg->Index, smsdtNormal);
		if (ret != 0)
			log_error("Unable to delete SMS on index %i: %i", Msg->Index, ret);

		field_array_free(arr, arrSize);
	} else log_error("Unable to get SMS arguments: %i", ret);

	log_exit("%i");
	return ret;
}


static int _notify_callback(const char* Line, void* Context)
{
	int ret = 0;
	size_t len = 0;
	char** arr = NULL;
	size_t arrSize = 0;
	int smsIndex = 0;
	int serialFD = -1;
	SMS_MESSAGE msg;
	log_enter("Line=0x%p; Context=0x%p", Line, Context);

	serialFD = (int)Context;
	len = strlen("+CMTI: ");
	if (strlen(Line) >= len && memcmp(Line, "+CMTI: ", sizeof("+CMTI: ") - 1) == 0) {
		Line += len;
		ret = field_array_get(Line, ',', &arr, &arrSize);
		if (ret == 0) {
			if (arrSize >= 2) {
				smsIndex = atoi(arr[1]);
				log_info("New message: Storage = %s, index = %i", arr[0], smsIndex);
				ret = command_sms_read(serialFD, smsIndex, &msg);
				if (ret == 0)
					_sms_process(serialFD, &msg);
				else log_error("Unable to read SMS on index %i: %i", smsIndex, ret);
			} else log_error("No SMS index present", );

			field_array_free(arr, arrSize);
		} else log_error("Unable to get notification fields: %i", ret);
	}

	log_exit("%i", ret);
	return ret;
}


int main(int argc, char **argv)
{
	int ret = 0;
	int serialFD = 0;

	ret = line_buffer_init();
	if (ret != 0) {
		log_error("Unable to initialize Line Buffer: %i", ret);
		return ret;
	}

	ret = process_command_line(argc, argv);
	if (ret == 0)
		ret = accounts_init();

	if (ret == 0) {
		int baudrate = 0;
		char* dn = NULL;

		settings_print(stderr);
		ret = settings_value_get_string("device", 0, &dn, "/dev/ttyS0");
		if (ret == 0)
			ret = settings_value_get_int("baudrate", 0, &baudrate, 115200);

		if (ret == 0)
			ret = serial_open(dn, baudrate, &serialFD);
		
		if (ret == 0) {
			int pinRequired = 0;
			PSMS_MESSAGE msgs = NULL;
			size_t msgCount = 0;

			ret = command_pin_required(serialFD, &pinRequired);
			if (ret == 0 && pinRequired) {
				char* pin = NULL;

				fprintf(stderr, "PIN is required\n");
				ret = settings_value_get_string("pin", 0, &pin, NULL);
				if (ret != 0) {
					ret = -1;
					fprintf(stderr, "PIN not specified (use -p <string>)\n");
				}

				if (ret == 0) {
					ret = command_pin_enter(serialFD, pin);
					if (ret != 0)
						log_error("Invalid PIN %s", pin);
				
					if (ret == 0)
						sleep(10);
				}
			}

			if (ret == 0) {
				ret = command_set_text_mode(serialFD, 1);
				if (ret != 0)
					log_error("Unable to enable SMS text mode: %i", ret);
			}

			if (ret == 0) {
				int gps = 0;
				int gprs = 0;

				ret = settings_value_get_int("gps", 0, &gps, 0);
				if (ret != 0)
					log_error("Unable to load GNSS status: %i", ret);

				ret = command_gnss_enable(serialFD, gps);
				if (ret != 0)
					log_error("Unable to set GPS state: %i", ret);
			
				ret = settings_value_get_int("gprs", 0, &gprs, 0);
				if (ret != 0)
					log_error("Unable to load GPRS status: %i", ret);

				ret = command_gprs_connect(serialFD, gprs);
				if (ret != 0)
					log_error("Unable to set GPRS state: %i", ret);
			}

			{
				char* cf = NULL;

				ret = settings_value_get_int("gpsperiod", 0, &_gpsPeriod, 30);
				if (ret != 0) {
					_gpsPeriod = 30;
					log_error("Unable to get GPS period: %i", ret);
				}

				ret = settings_value_get_int("syncperiod", 0, &_syncPeriod, 300);
				if (ret != 0) {
					_syncPeriod = 300;
					log_error("Unable to get sync period: %i", ret);
				}

				ret = settings_value_get_string("logfile", 0, &cf, "gpsapp.log");
				if (ret != 0)
					log_error("Unable to set log file: %i", ret);

				ret = settings_value_get_string("gpsfile", 0, &cf, "gpsapp.gps");
				if (ret != 0)
					log_error("Unable to set GPS file: %i", ret);

				ret = settings_save(_configFile, ':');
				if (ret != 0)
					log_error("Unable to save the settings: %i", ret);
			}

			ret = line_callback_register(_notify_callback, (void *)serialFD, &_notifyCallbackHandle);
			if (ret == 0) {
				ret = command_sms_list(serialFD, "ALL", &msgs, &msgCount);
				if (ret != 0)
					log_error("Unable to list SMS messages: %i", ret);

				if (ret == 0) {
					fprintf(stderr, "%zu messages\n", msgCount);
					for (size_t i = 0; i < msgCount; ++i)
						_sms_process(serialFD, msgs + i);

					sms_array_free(msgs, msgCount);
				}

				for (;;) {
					int gnssStatus = 0;
					GPS_RECORD gpsRecord;
					int timeUnit = 10;

					serial_response_wait(serialFD, timeUnit, 0, NULL, NULL);
					if (_gpsPeriod >= timeUnit)
						_gpsPeriod -= timeUnit;
					else _gpsPeriod = 0;

					if (_gpsPeriod == 0) {
						ret = command_gnss_status(serialFD, &gnssStatus);
						if (ret == 0) {
							if (!gnssStatus) {
								ret = command_gnss_enable(serialFD, 1);
								if (ret != 0)
									log_error("Unable to enable GNSS: %i", ret);
							
								serial_response_wait(serialFD, 60, 0, NULL, NULL);
							}

							if (ret == 0) {
								ret = command_gnss_info(serialFD, &gpsRecord);
								if (ret != 0)
									log_error("Unable to get GNSS location: %i", ret);

								if (ret == 0 && gpsRecord.FixStatus == 1) {
									char msg[1024];

									snprintf(msg, sizeof(msg) / sizeof(msg[0]), "%lf %lf %s", gpsRecord.Lattitude, gpsRecord.Longitude, gpsRecord.Timestamp);
									ret = settings_value_add("loc", msg);
									if (ret != 0)
										log_error("Unable to remember the GPS value: %i", ret);

									if (ret == 0) {
										ret = settings_save(_configFile, ':');
										if (ret == EINVAL)
											ret = 0;

										if (ret != 0)
											log_error("Unable to save settings: %i", ret);
									}

									command_gnss_info_free(&gpsRecord);
								}

								if (!gnssStatus) {
									ret = command_gnss_enable(serialFD, 0);
									if (ret != 0)
										log_error("Unable to disable GNSS: %i", ret);
								}
							}
						} else log_error("Unable to get GNSS status: %i", ret);					

						ret = settings_value_get_int("gpsperiod", 0, &_gpsPeriod, 30);
						if (ret != 0)
							log_error("Unable to get GPS period: %i", ret);
					}

					if (_syncPeriod >= timeUnit)
						_syncPeriod -= timeUnit;
					else _syncPeriod = 0;

					if (_syncPeriod == 0) {
						int gprsEnabled = 0;

						ret = command_gprs_connected(serialFD, &gprsEnabled);
						if (ret != 0)
							log_error("Unable to get GPRS status: %i", ret);

						if (ret == 0 && gprsEnabled) {
							// TODO: Do the synchronization
							ret = settings_key_delete("loc");
							if (ret != 0)
								log_error("Unable to delete the GPS location data: %i", ret);
						}

						ret = settings_value_get_int("syncperiod", 0, &_syncPeriod, 300);
						if (ret != 0)
							log_error("Unable to get sync period: %i", ret);
					}
					
					fputc('.', stderr);
				}

				line_callback_unregister(_notifyCallbackHandle);
			} else log_error("Unable to register Line Buffer callback: %i", ret);

			serial_close(serialFD);
		} else {
			log_error("Unable to open serial \"%s\": %i", dn, ret);
		}
	}

	line_buffer_finit();
	accounts_finit();
	settings_free();

	return ret;
}
