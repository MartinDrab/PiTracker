
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "logging.h"
#include "settings.h"
#include "field-array.h"
#include "accounts.h"




static PACCOUNT_RECORD _accounts = NULL;
static size_t _accountCount = 0;



static PACCOUNT_RECORD _account_get(const char* Login)
{
	PACCOUNT_RECORD ret = NULL;
	log_enter("Login=\"%s\"", Login);

	log_exit("0x%p", ret);
	return ret;
}


int account_add(const char* Login, const char* Password, int Admin)
{
	int ret = 0;
	char* l = NULL;
	char* p = NULL;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"; Password=\"%s\"; Admin=%u", Login, Password, Admin);

	tmp = _account_get(Login);
	if (tmp == NULL) {
		l = strdup(Login);
		if (l == NULL)
			ret = ENOMEM;

		if (ret == 0) {
			p = strdup(Password);
			if (p == NULL)
				ret = ENOMEM;
		}

		if (ret == 0) {
			tmp = realloc(_accounts, (_accountCount + 1) * sizeof(ACCOUNT_RECORD));
			if (tmp != NULL) {
				_accounts = tmp;
				tmp = _accounts + _accountCount;
				tmp->Login = l;
				tmp->Password = p;
				tmp->Authenticated = 0;
				tmp->Admin = Admin;
				++_accountCount;
			} else ret = ENOMEM;
		}

		if (ret != 0) {
			free(p);
			free(l);
		}
	} else ret = EEXIST;

	log_exit("%i", ret);
	return ret;
}


int account_delete(const char* Login, const char* Password)
{
	int ret = 0;
	size_t index = 0;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"; Password=\"%s\"", Login, Password);

	tmp = _account_get(Login);
	if (tmp != NULL) {
		if (strcmp(tmp->Password, Password) == 0) {
			index = (size_t)(tmp - _accounts);
			free(tmp->Login);
			free(tmp->Password);
			memmove(tmp, tmp + 1, (_accountCount - index - 1) * sizeof(ACCOUNT_RECORD));
			--_accountCount;
		} else ret = EACCES;
	} else ret = ENOENT;

	log_exit("%i", ret);
	return ret;
}


int account_login(const char* Login, const char* Password)
{
	int ret = 0;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"; Password=\"%s\"", Login, Password);

	tmp = _account_get(Login);
	if (tmp != NULL) {
		if (strcmp(tmp->Password, Password) == 0) {
			if (tmp->Authenticated)
				ret = EEXIST;
			
			tmp->Authenticated = 1;
		} else ret = EACCES;
	} else ret = ENOENT;

	log_exit("", ret);
	return ret;
}

int account_logout(const char* Login)
{
	int ret = 0;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"", Login);

	tmp = _account_get(Login);
	if (tmp != NULL) {
		if (!tmp->Authenticated)
			ret = EEXIST;

		tmp->Authenticated = 0;
	} else ret = ENOENT;

	log_exit("%i", ret);
	return ret;
}


int account_logged_in(const char* Login, int* Result)
{
	int ret = 0;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"; Result=0x%p", Login, Result);

	tmp = _account_get(Login);
	if (tmp != NULL)
		*Result = tmp->Authenticated;
	else ret = ENOENT;

	log_exit("%i, *Result=%i", ret, *Result);
	return ret;
}


int account_set_password(const char* Login, const char* Old, const char* New)
{
	int ret = 0;
	char* p = NULL;
	PACCOUNT_RECORD tmp = NULL;
	log_enter("Login=\"%s\"; Old=\"%s\"; New=\"%s\"", Login, Old, New);

	tmp = _account_get(Login);
	if (tmp != NULL) {
		p = strdup(New);
		if (p == NULL)
			ret = ENOMEM;

		if (ret == 0) {
			if (strcmp(Old, tmp->Password) != 0)
				ret = EACCES;

			if (ret == 0) {
				free(tmp->Password);
				tmp->Password = p;
			}

			if (ret != 0)
				free(p);
		}
	} else ret = ENOENT;

	log_exit("%i", ret);
	return ret;
}


int accounts_init(void)
{
	int ret = 0;
	char** lines = NULL;
	size_t lineCount = 0;
	log_enter("");

	ret = settings_values_enum("account", &lines, &lineCount);
	if (ret == ENOENT) {
		ret = settings_value_add("account", "* 123456");
		if (ret == 0)
			ret = settings_values_enum("account", &lines, &lineCount);
	}

	if (ret == 0) {
		for (size_t i = 0; i < lineCount; ++i) {
			char** arr = NULL;
			size_t arrSize = 0;

			ret = field_array_get(lines[i], ' ', &arr, &arrSize);
			if (ret == 0) {
				if (arrSize >= 2) {
					int admin = 0;

					if (arrSize >= 3)
						admin = atoi(arr[2]);

					ret = account_add(arr[0], arr[1], admin);
					if (ret != 0)
						log_error("Unable to add account %s:%s:%i: %i", arr[0], arr[1], admin, ret);
				} else log_error("Invalid value: \"%s\"", lines[i]);

				field_array_free(arr, arrSize);
			}

			if (ret != 0)
				break;
		}

		settings_values_free(lines, lineCount);
	}

	log_exit("%i", ret);
	return ret;
}


int accounts_save(void)
{
	int ret = 0;
	char line[256];
	PACCOUNT_RECORD tmp = NULL;
	log_enter("");

	ret = settings_key_delete("account");
	if (ret == ENOENT)
		ret = 0;

	if (ret == 0) {
		tmp = _accounts;
		for (size_t i = 0; i < _accountCount; ++i) {
			snprintf(line, sizeof(line) / sizeof(line[0]), "%s %s %i", tmp->Login, tmp->Password, tmp->Admin);
			ret = settings_value_add("account", line);
			if (ret != 0)
				break;
		}
	}

	log_exit("%i", ret);
	return ret;
}


void accounts_finit(void)
{
	PACCOUNT_RECORD tmp = NULL;
	log_enter("");

	tmp = _accounts;
	for (size_t i = 0; i < _accountCount; ++i) {
		free(tmp->Login);
		free(tmp->Password);
		++tmp;
	}

	free(_accounts);
	_accounts = NULL;
	_accountCount = 0;

	log_exit("void");
	return;
}
