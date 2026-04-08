#ifndef COMMONLIB_ERROR_H
#define COMMONLIB_ERROR_H

/* Module was modified to remove ERROR_TRACE and ERROR_PRINT, and return error code */

#include "bool.h"
#include "macro.h"

/*#define ERROR_EMBED_ARGUMENTS*/ /* Store file and line as a part of the message (increases file size) */
#define ERROR_INCLUDE_EXPRESSION /* Include boolean expression in the trace */

#ifdef ERROR_EMBED_ARGUMENTS
    #define ERROR_FORMAT() __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Error"
    #define ERROR_FORMAT_F(FORMAT) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Message: " FORMAT
    #ifdef ERROR_INCLUDE_EXPRESSION
        #define ERROR_FORMAT_E(EXPRESSION) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Condition `" EXPRESSION "' failed"
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Condition `" EXPRESSION "' failed. Message: " FORMAT
    #else
        #define ERROR_FORMAT_E(EXPRESSION) ERROR_FORMAT()
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) ERROR_FORMAT_F(FORMAT)
    #endif
#else
    #define ERROR_FORMAT() "%s:%d: Error", __BASENAME_FILE__, __LINE__
    #define ERROR_FORMAT_F(FORMAT) "%s:%d: Message: " FORMAT, __BASENAME_FILE__, __LINE__
    #ifdef ERROR_INCLUDE_EXPRESSION
        #define ERROR_FORMAT_E(EXPRESSION) "%s:%d: Condition `%s' failed", __BASENAME_FILE__, __LINE__, EXPRESSION
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) "%s:%d: Condition `%s' failed. Message: " FORMAT, __BASENAME_FILE__, __LINE__, EXPRESSION
    #else
        #define ERROR_FORMAT_E(EXPRESSION) ERROR_FORMAT()
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) ERROR_FORMAT_F(FORMAT)
    #endif
#endif

/* Essential macros */
#define ERROR_TYPE void
#define ERROR_DECLARE()
#define ERROR_ASSIGN(EXPRESSION) EXPRESSION
#define ERROR_RETURN() return
#define ERROR_RETURN_VERBATIM() return
#define ERROR_RETURN_OK() return

/* Assigns 'error' variable and goes to 'failure' label (GOTO = goto) */
#define GOTO(CODE) { error_print_die(CODE, ERROR_FORMAT()); }
#define GOTO0(CODE, FORMAT) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT)); }
#define GOTO1(CODE, FORMAT, A) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A); }
#define GOTO2(CODE, FORMAT, A, B) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B); }
#define GOTO3(CODE, FORMAT, A, B, C) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B, C); }

/* Assigns 'error' variable and goes to 'failure' label if expression is false (AGOTO = assert goto) */
#define AGOTO(CODE, EXPRESSION) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT()); }
#define AGOTO0(CODE, EXPRESSION, FORMAT) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT)); }
#define AGOTO1(CODE, EXPRESSION, FORMAT, A) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A); }
#define AGOTO2(CODE, EXPRESSION, FORMAT, A, B) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B); }
#define AGOTO3(CODE, EXPRESSION, FORMAT, A, B, C) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B, C); }

/* Assigns 'error' variable and goes to 'failure' label if expression is error (PGOTO = propagate goto) */
#define PGOTO(EXPRESSION) { EXPRESSION; }
#define PGOTO0(EXPRESSION, FORMAT) { EXPRESSION; }
#define PGOTO1(EXPRESSION, FORMAT, A) { EXPRESSION; }
#define PGOTO2(EXPRESSION, FORMAT, A, B) { EXPRESSION; }
#define PGOTO3(EXPRESSION, FORMAT, A, B, C) { EXPRESSION; }

/* Returns the error (RET = return) */
#define RET(CODE) { error_print_die(CODE, ERROR_FORMAT()); }
#define RET0(CODE, FORMAT) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT)); }
#define RET1(CODE, FORMAT, A) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A); }
#define RET2(CODE, FORMAT, A, B) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B); }
#define RET3(CODE, FORMAT, A, B, C) { error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B, C); }

/* Returns error if expression is false (ARET = assert return) */
#define ARET(CODE, EXPRESSION) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT()); }
#define ARET0(CODE, EXPRESSION, FORMAT) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT)); }
#define ARET1(CODE, EXPRESSION, FORMAT, A) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A); }
#define ARET2(CODE, EXPRESSION, FORMAT, A, B) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B); }
#define ARET3(CODE, EXPRESSION, FORMAT, A, B, C) { const bool check = EXPRESSION; if (check) {} else error_print_die(CODE, ERROR_FORMAT_F(FORMAT), A, B, C); }

/* Returns error if expression is error (PRET = propagate return) */
#define PRET(EXPRESSION) { EXPRESSION; }
#define PRET0(EXPRESSION, FORMAT) { EXPRESSION; }
#define PRET1(EXPRESSION, FORMAT, A) { EXPRESSION; }
#define PRET2(EXPRESSION, FORMAT, A, B) { EXPRESSION; }
#define PRET3(EXPRESSION, FORMAT, A, B, C) { EXPRESSION; }

/* Ignores the result of expression when it is guaranteed to succeed (PIGNORE = propagate ignore) */
#define PIGNORE(EXPRESSION) { EXPRESSION; }

/* Prints error and dies */
void error_print_die(int code, const char *format, ...) NORETURN PRINTFLIKE(2, 3);

#endif
