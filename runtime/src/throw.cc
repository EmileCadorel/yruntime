#include <setjmp.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <type.hh>

volatile int _yrt_exc_handled;

volatile unsigned _yrt_exc_tries;

char *_yrt_exc_file;
char *_yrt_exc_function;
unsigned _yrt_exc_line;
volatile int _yrt_exc_code;

TypeInfo _yrt_exc_type_info;
void* _yrt_exc_type_data;

/* This is the global stack of catchers. */
struct _yrt_exc_stack *_yrt_exc_global;

/* Stack is actually a linked list of catcher cells. */
struct _yrt_exc_stack
{
    unsigned num;
    jmp_buf j;
    struct _yrt_exc_stack *prev;
};

char* toUtf8 (unsigned int code, char chars[5]);

extern "C" void _yrt_exc_print (FILE *stream, char *file, char *function, unsigned line,
		   int code)
{
    fprintf (stream, "Exception in file \"%s\", at line %u",
	     file, line);
    
    if (function)
	{
	    fprintf (stream, ", in function \"%s\"", function);
	}
    fprintf (stream, ", of type ");    
    int * data = (int*) _yrt_exc_type_info.name.data;
    for (unsigned int i = 0 ; i < _yrt_exc_type_info.name.len ; i++) {
	char c[5];
	fprintf (stream, "%s", toUtf8 (data [i], c));
    }
    
    fprintf (stream, ".");
    
#ifdef _yrt_EXC_PRINT
    fprintf (stream, " Exception ");
    _yrt_EXC_PRINT (code, stream);
#endif
    fprintf (stream, "\n");
}

extern "C" int _yrt_exc_push (jmp_buf *j, int returned) {
    static _yrt_exc_stack *head;
    if (returned != 0) { // Le jmp a déjà été déclaré, on est revenu dessus suite à un throw
	return 0;	
    }

    ++ _yrt_exc_tries;
    /* Using memcpy here is the best alternative. */
    head = (_yrt_exc_stack*) malloc (sizeof (_yrt_exc_stack));
    memcpy (&head->j, j, sizeof (jmp_buf));
    head->num = _yrt_exc_tries;
    head->prev = _yrt_exc_global;
    _yrt_exc_global = head;

    return 1;     
}

extern "C" void _yrt_exc_pop (jmp_buf *j)
{
    struct _yrt_exc_stack *stored = _yrt_exc_global;

    if (stored == NULL)
	{	    
	    fprintf (stderr, "Unhandled exception\n");
	    _yrt_exc_print (stderr, _yrt_exc_file, _yrt_exc_function,
			 _yrt_exc_line, _yrt_exc_code);
	    
	    raise (SIGABRT);
	}
    
    _yrt_exc_global = stored->prev;
    
    if (j)
	{
	    /* This assumes that JMP_BUF is a structure etc. and can be
	       copied rawely.  This is true in GLIBC, as I know. */
	    memcpy (j, &stored->j, sizeof (jmp_buf));
	}
        
    /* While with MALLOC, free.  When using obstacks it is better not to
       free and hold up. */
    free (stored);
}

extern "C" void _yrt_exc_store (TypeInfo info, void* data) {
    _yrt_exc_type_info = info;
    _yrt_exc_type_data = data;
}

extern "C" void* _yrt_exc_check_type (TypeInfo info) {
    // core::typeinfo::equals (TypeInfo, TypeInfo)-> bool
    if (_Y4core8typeinfo6equalsF4core8typeinfo8TypeInfo4core8typeinfo8TypeInfoZb (_yrt_exc_type_info, info)) {
	return _yrt_exc_type_data;
    }
    return NULL;
}

extern "C" void _yrt_exc_throw (char *file, char *function, unsigned line, TypeInfo info, void* data)
{
    jmp_buf j;

    _yrt_exc_file = file;
    _yrt_exc_function = function;
    _yrt_exc_line = line;
    _yrt_exc_store (info, data);
    
    /* Pop for jumping. */
    _yrt_exc_pop (&j);
   
    /* LONGJUMP to J with nonzero value. */
    longjmp (j, 1);
}

extern "C" void _yrt_exc_rethrow () {
    jmp_buf j;
    _yrt_exc_pop (&j);
   
    /* LONGJUMP to J with nonzero value. */
    longjmp (j, 1);
}
