
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "logging.h"
#include "line-buffer.h"



#define LINE_BUFFER_SIZE			1024

static char _lineBuffer[LINE_BUFFER_SIZE];
static size_t _lineBufferIndex = 0;
static LINE_BUFFER_CALLBACK_RECORD _lineCallbackHead;
static void* _debugCallbackHandle = NULL;


static int _line_buffer_debug_callback(const char* Line, void* Context)
{
	int ret = 0;
	log_enter("Line=0x%p; Context=0x%p", Line, Context);

	log_exit("%i", ret);
	return ret;
}



int line_buffer_insert(const char* Data, size_t Length)
{
	int ret = 0;
	char* tmp = NULL;
	char* lineEnd = NULL;
	PLINE_BUFFER_CALLBACK_RECORD r = NULL;
	PLINE_BUFFER_CALLBACK_RECORD old = NULL;
	log_enter("Data=0x%p; Length=%zu", Data, Length);

	if (_lineBufferIndex + Length + 1 < LINE_BUFFER_SIZE) {
		memcpy(_lineBuffer + _lineBufferIndex, Data, Length);
		_lineBufferIndex += Length;
		_lineBuffer[_lineBufferIndex] = '\0';
		lineEnd = strstr(_lineBuffer, "\r\n");
		while (lineEnd != NULL) {
			lineEnd[0] = '\0';
			lineEnd[1] = '\0';
			lineEnd += 2;
			tmp = _lineBuffer;
			while (*tmp == '\r' || *tmp == '\n')
				++tmp;

			r = _lineCallbackHead.Next;
			while (r != &_lineCallbackHead) {
				old = r;
				r = r->Next;
				if (old->Enabled)
					old->Callback(tmp, old->Context);				
			}

			memmove(_lineBuffer, lineEnd, (_lineBufferIndex - (size_t)(lineEnd - _lineBuffer))*sizeof(char));
			_lineBufferIndex -= (size_t)(lineEnd - _lineBuffer);
			_lineBuffer[_lineBufferIndex] = '\0';
			lineEnd = strstr(_lineBuffer, "\r\n");
		}
	} else ret = ENOMEM;

	log_exit("%i", ret);
	return ret;
}


int line_callback_register(LINE_BUFFER_CALLBACK* Callback, void* Context, void** Handle)
{
	int ret = 0;
	PLINE_BUFFER_CALLBACK_RECORD record = NULL;
	log_enter("Callback=0x%p; Context=0x%p; Handle=0x%p", Callback, Context, Handle);
	
	record = malloc(sizeof(LINE_BUFFER_CALLBACK_RECORD));
	if (record != NULL) {
		memset(record, 0, sizeof(LINE_BUFFER_CALLBACK_RECORD));
		record->Callback = Callback;
		record->Context = Context;
		record->Enabled = 1;
		record->Next = &_lineCallbackHead;
		record->Prev = _lineCallbackHead.Prev;
		_lineCallbackHead.Prev->Next = record;
		_lineCallbackHead.Prev = record;
		*Handle = record;
	} else ret = ENOMEM;

	log_exit("%i, *Handle=0x%p", ret, *Handle);
	return ret;
}

void line_callback_unregister(void* Handle)
{
	PLINE_BUFFER_CALLBACK_RECORD record = NULL;
	log_enter("Handle=0x%p", Handle);

	record = (PLINE_BUFFER_CALLBACK_RECORD)Handle;
	record->Prev->Next = record->Next;
	record->Next->Prev = record->Prev;
	free(record);

	log_exit("void");
	return;
}


void line_callback_enable(void* Handle, int Enable)
{
	PLINE_BUFFER_CALLBACK_RECORD record = NULL;
	log_enter("Handle=0x%p; Enable=%i", Handle, Enable);

	record = (PLINE_BUFFER_CALLBACK_RECORD)Handle;
	record->Enabled = Enable;

	log_exit("void");
	return;
}


int line_buffer_init(void)
{
	int ret = 0;
	log_enter("");

	_lineCallbackHead.Next = &_lineCallbackHead;
	_lineCallbackHead.Prev = &_lineCallbackHead;
	ret = line_callback_register(_line_buffer_debug_callback, &_lineBufferIndex, &_debugCallbackHandle);

	log_exit("%i", ret);
	return ret;
}


void line_buffer_finit(void)
{
	PLINE_BUFFER_CALLBACK_RECORD r = NULL;
	PLINE_BUFFER_CALLBACK_RECORD old = NULL;
	log_enter("");

	line_callback_unregister(_debugCallbackHandle);
	r = _lineCallbackHead.Next;
	while (r != &_lineCallbackHead) {
		old = r;
		r = r->Next;
		free(old);
	}

	log_exit("void");
	return;
}
