
#pragma once




typedef int (LINE_BUFFER_CALLBACK)(const char *Line, void *Context);

typedef struct _LINE_BUFFER_CALLBACK_RECORD {
	struct _LINE_BUFFER_CALLBACK_RECORD* Next;
	struct _LINE_BUFFER_CALLBACK_RECORD* Prev;
	LINE_BUFFER_CALLBACK* Callback;
	void* Context;
	int Enabled;
} LINE_BUFFER_CALLBACK_RECORD, *PLINE_BUFFER_CALLBACK_RECORD;


int line_buffer_insert(const char* Data, size_t Length);
int line_callback_register(LINE_BUFFER_CALLBACK* Callback, void* Context, void** Handle);
void line_callback_unregister(void* Handle);
void line_callback_enable(void* Handle, int Enable);

int line_buffer_init(void);
void line_buffer_finit(void);
