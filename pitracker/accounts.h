
#pragma once




typedef struct _ACCOUNT_RECORD {
	char *Login;
	char *Password;
	int Authenticated;
	int Admin;
} ACCOUNT_RECORD, *PACCOUNT_RECORD;



int account_add(const char* Login, const char* Password, int Admin);
int account_delete(const char* Login, const char* Password);
int account_login(const char* Login, const char* Password);
int account_logout(const char* Login);
int account_logged_in(const char* Login, int* Result);
int account_set_password(const char* Login, const char* Old, const char* New);

int accounts_init(void);
void accounts_finit(void);
int accounts_save(void);
