#ifndef PL_UTIL_H_
#define PL_UTIL_H_

#define UNUSED_ARG(x) (void) x

/* Get 'now' timestamp (microseconds since 1970) */
double now_us(void);

/* Get how many memory pages are currently in use */
long total_memory_pages(void);

/*
 * Deferred errors.
 *
 * croak() must NEVER be called while V8 objects with destructors
 * (HandleScope, Isolate::Scope, Context::Scope, TryCatch, ...) are on the
 * C++ stack: croak() longjmp()s past them, their destructors never run, and
 * the isolate is left corrupt (still "entered", stale handle scope data).
 * The process then dies later: an abort in Isolate::Dispose() ("Disposing
 * the isolate that is entered by a thread") or a SIGSEGV in the next GC
 * (Relocatable::Iterate).
 *
 * Code running under V8 scopes therefore records the error here and returns.
 * V8Context methods raise it with croak() once every scope has been closed,
 * and perl_caller() turns it into a JS exception.  Only the first error is
 * kept until it is cleared.
 */
void pl_set_pending_error(const char* fmt, ...);
int pl_has_pending_error(void);
const char* pl_get_pending_error(void);
void pl_clear_pending_error(void);

#endif
