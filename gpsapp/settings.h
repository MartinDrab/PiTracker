
#pragma once



int settings_keys_enum(char ***Keys, size_t *Count);
int settings_key_add(const char* Key);
int settings_key_delete(const char* Key);
void settings_keys_free(char** Keys, size_t Count);
int settings_values_enum(const char* Key, char*** Values, size_t* Count);
int settings_value_count(const char *Key, size_t *Count);
int settings_value_add(const char* Key, const char *Value);
int settings_value_get_string(const char* Key, size_t Index, char** Value, const char* Default);
int settings_value_get_int(const char* Key, size_t Index, int* Value, int Default);
int settings_value_set_string(const char *Key, size_t Index, const char *Value);
int settings_value_set_int(const char* Key, size_t Index, int Value);
int settings_value_delete(const char* Key, size_t Index);
void settings_values_free(char** Values, size_t Count);
int settings_params_enum(const char *Key, size_t ValueIndex, char ***Params, size_t *Count);
void settings_params_free(char** Params, size_t Count);

int settings_load(const char* FileName, char Delimiter, char Comment);
int settings_save(const char* FileName, char Delimiter);
void settings_free(void);
void settings_print(FILE* Stream);

