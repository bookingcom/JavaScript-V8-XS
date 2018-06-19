#ifndef PL_V8_H
#define PL_V8_H

#include <v8.h>
#include "pl_config.h"

class V8Context;

#if 0
#define V8_OPT_NAME_GATHER_STATS      "gather_stats"
#define V8_OPT_NAME_SAVE_MESSAGES     "save_messages"
#define V8_OPT_NAME_MAX_MEMORY_BYTES  "max_memory_bytes"
#define V8_OPT_NAME_MAX_TIMEOUT_US    "max_timeout_us"

#define V8_OPT_FLAG_GATHER_STATS      0x01
#define V8_OPT_FLAG_SAVE_MESSAGES     0x02
#define V8_OPT_FLAG_MAX_MEMORY_BYTES  0x04
#define V8_OPT_FLAG_MAX_TIMEOUT_US    0x08

#define PL_NAME_ROOT              "_perl_"
#define PL_NAME_GENERIC_CALLBACK  "generic_callback"

#define PL_SLOT_CREATE(name)      (PL_NAME_ROOT "." #name)

#define PL_SLOT_GENERIC_CALLBACK  PL_SLOT_CREATE(PL_NAME_GENERIC_CALLBACK)
#endif

/*
 * We use these two functions to convert back and forth between the Perl
 * representation of an object and the JS one.
 *
 * Because data in Perl and JS can be nested (array of hashes of arrays of...),
 * the functions are recursive.
 *
 * pl_duk_to_perl: takes a JS value from a given position in the duktape stack,
 * and creates the equivalent Perl value.
 *
 * pl_perl_to_duk: takes a Perl value and leaves the equivalent JS value at the
 * top of the duktape stack.
 */
SV* pl_v8_to_perl(pTHX_ V8Context* ctx, const v8::Handle<v8::Object>& object);
const v8::Handle<v8::Object> pl_perl_to_v8(pTHX_ SV* value, V8Context* ctx);

/*
 * Get the JS value of a global / nested property as Perl data.
 */
SV* pl_get_global_or_property(pTHX_ V8Context* ctx, const char* name);

/*
 * Return true if a given global / nested property exists.
 */
SV* pl_exists_global_or_property(pTHX_ V8Context* ctx, const char* name);

/*
 * Get the JS type of a global / nested property as a Perl string.
 */
SV* pl_typeof_global_or_property(pTHX_ V8Context* ctx, const char* name);

/*
 * Set the JS value of a global / nested property from Perl data.
 */
int pl_set_global_or_property(pTHX_ V8Context* ctx, const char* name, SV* value);

/*
 * Run a piece of JS code, associated with an (optional) file name.
 */
SV* pl_eval(pTHX_ V8Context* ctx, const char* code, const char* file = 0);

/*
 * Run the V8 GC
 */
int pl_run_gc(V8Context* ctx);

#if 0
/*
 * This is a generic dispatcher that allows calling any Perl function from JS.
 */
int pl_call_perl_sv(duk_context* ctx, SV* func);

/* Get / set the value for a global object or a slot in an object */
SV* pl_instanceof_global_or_property(pTHX_ duk_context* ctx, const char* object, const char* class);
#endif

#endif
