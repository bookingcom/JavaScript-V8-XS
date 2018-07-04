#include "libplatform/libplatform.h"
#include "pl_util.h"
#include "pl_v8.h"
#include "pl_eval.h"
#include "pl_native.h"
#include "pl_console.h"
#include "pl_eventloop.h"
#include "pl_inlined.h"
#include "pl_stats.h"
#include "V8Context.h"

#define MAX_MEMORY_MINIMUM  (128 * 1024) /* 128 KB */
#define MAX_TIMEOUT_MINIMUM (500000)     /* 500_000 us = 500 ms = 0.5 s */

#define ENTER_SCOPE \
    Isolate::Scope isolate_scope(isolate); \
    HandleScope handle_scope(isolate)

int V8Context::instance_count = 0;
std::unique_ptr<Platform> V8Context::platform = 0;

V8Context::V8Context(HV* opt)
    : inited(0)
{
    program = new char[256];
    sprintf(program, "program_%05d", instance_count);

    V8Context::initialize_v8(this);

    // Create a new Isolate and make it the current one.
    create_params.array_buffer_allocator =
        ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = Isolate::New(create_params);

    ENTER_SCOPE;

    pagesize_bytes = total_memory_pages();
    stats = newHV();
    msgs = newHV();
    flags = 0;

    if (opt) {
        hv_iterinit(opt);
        while (1) {
            SV* value = 0;
            I32 klen = 0;
            char* kstr = 0;
            HE* entry = hv_iternext(opt);
            if (!entry) {
                break; /* no more hash keys */
            }
            kstr = hv_iterkey(entry, &klen);
            if (!kstr || klen < 0) {
                continue; /* invalid key */
            }
            value = hv_iterval(opt, entry);
            if (!value) {
                continue; /* invalid value */
            }
            if (memcmp(kstr, V8_OPT_NAME_GATHER_STATS, klen) == 0) {
                flags |= SvTRUE(value) ? V8_OPT_FLAG_GATHER_STATS : 0;
                continue;
            }
            if (memcmp(kstr, V8_OPT_NAME_SAVE_MESSAGES, klen) == 0) {
                flags |= SvTRUE(value) ? V8_OPT_FLAG_SAVE_MESSAGES : 0;
                continue;
            }
            if (memcmp(kstr, V8_OPT_NAME_MAX_MEMORY_BYTES, klen) == 0) {
                int param = SvIV(value);
                max_allocated_bytes = param > MAX_MEMORY_MINIMUM ? param : MAX_MEMORY_MINIMUM;
                continue;
            }
            if (memcmp(kstr, V8_OPT_NAME_MAX_TIMEOUT_US, klen) == 0) {
                int param = SvIV(value);
                max_timeout_us = param > MAX_TIMEOUT_MINIMUM ? param : MAX_TIMEOUT_MINIMUM;
                continue;
            }
            croak("Unknown option %*.*s\n", (int) klen, (int) klen, kstr);
        }
    }

    // Create a template for the global object.
    Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate);

    // Register some callbacks to native functions
    pl_register_native_functions(this, object_template);
    // pl_register_eventloop_functions(this);

    // Register inlined JS code
    // pl_register_inlined_functions(this);

    // Create a new context.
    Local<Context> context = Context::New(isolate, 0, object_template);
    persistent_context.Reset(isolate, context);
    persistent_template.Reset(isolate, object_template);
}

V8Context::~V8Context()
{
    isolate->Dispose();
    delete create_params.array_buffer_allocator;
    delete[] program;

#if 0
    // We should terminate v8 at some point.  However, because the calling code
    // may create multiple instances of V8Context, whether "nested" or
    // "sequential", we cannot just assume we should do this.  For now, we just
    // *never* terminate v8.
    V8Context::terminate_v8(this);
#endif
}

SV* V8Context::get(const char* name)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    SV* ret = pl_get_global_or_property(aTHX_ this, name);
    pl_stats_stop(aTHX_ this, &perf, "get");
    return ret;
}

SV* V8Context::exists(const char* name)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    SV* ret = pl_exists_global_or_property(aTHX_ this, name);
    pl_stats_stop(aTHX_ this, &perf, "exists");
    return ret;
}

SV* V8Context::typeof(const char* name)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    SV* ret = pl_typeof_global_or_property(aTHX_ this, name);
    pl_stats_stop(aTHX_ this, &perf, "typeof");
    return ret;
}

SV* V8Context::instanceof(const char* oname, const char* cname)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    SV* ret = pl_instanceof_global_or_property(aTHX_ this, oname, cname);
    pl_stats_stop(aTHX_ this, &perf, "instanceof");
    return ret;
}

void V8Context::set(const char* name, SV* value)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    pl_set_global_or_property(aTHX_ this, name, value);
    pl_stats_stop(aTHX_ this, &perf, "set");
}

SV* V8Context::eval(const char* code, const char* file)
{
    ENTER_SCOPE;
    register_functions();

    // performance is tracked inside this call
    return pl_eval(aTHX_ this, code, file);
}

SV* V8Context::dispatch_function_in_event_loop(const char* func)
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    SV* ret = pl_run_function_in_event_loop(aTHX_ this, func);
    pl_stats_stop(aTHX_ this, &perf, "dispatch");
    return ret;
}

int V8Context::run_gc()
{
    ENTER_SCOPE;
    register_functions();

    Perf perf;
    pl_stats_start(aTHX_ this, &perf);
    int ret = pl_run_gc(this);
    pl_stats_stop(aTHX_ this, &perf, "run_gc");
    return ret;
}

HV* V8Context::get_stats()
{
    return stats;
}

void V8Context::reset_stats()
{
    stats = newHV();
}

HV* V8Context::get_msgs()
{
    return msgs;
}

void V8Context::reset_msgs()
{
    msgs = newHV();
}

void V8Context::register_functions()
{
    // TODO: this is here because it cannot be done at construction time
    if (inited) {
        return;
    }
    inited = 1;
    ENTER_SCOPE;
    pl_register_eventloop_functions(this);
    pl_register_inlined_functions(this);
    pl_register_console_functions(this);
}

void V8Context::initialize_v8(V8Context* self)
{
    if (instance_count++) {
        return;
    }
    V8::InitializeICUDefaultLocation(self->program);
    V8::InitializeExternalStartupData(self->program);
    platform = platform::NewDefaultPlatform();
    V8::InitializePlatform(platform.get());
    V8::Initialize();
}

void V8Context::terminate_v8(V8Context* self)
{
    if (--instance_count) {
        return;
    }
    V8::Dispose();
    V8::ShutdownPlatform();
}

uint64_t V8Context::GetTypeFlags(const Local<Value>& v)
{
    uint64_t result = 0;
    if (v->IsArgumentsObject()  ) result |= 0x0000000000000001;
    if (v->IsArrayBuffer()      ) result |= 0x0000000000000002;
    if (v->IsArrayBufferView()  ) result |= 0x0000000000000004;
    if (v->IsArray()            ) result |= 0x0000000000000008;
    if (v->IsBooleanObject()    ) result |= 0x0000000000000010;
    if (v->IsBoolean()          ) result |= 0x0000000000000020;
    if (v->IsDataView()         ) result |= 0x0000000000000040;
    if (v->IsDate()             ) result |= 0x0000000000000080;
    if (v->IsExternal()         ) result |= 0x0000000000000100;
    if (v->IsFalse()            ) result |= 0x0000000000000200;
    if (v->IsFloat32Array()     ) result |= 0x0000000000000400;
    if (v->IsFloat64Array()     ) result |= 0x0000000000000800;
    if (v->IsFunction()         ) result |= 0x0000000000001000;
    if (v->IsGeneratorFunction()) result |= 0x0000000000002000;
    if (v->IsGeneratorObject()  ) result |= 0x0000000000004000;
    if (v->IsInt16Array()       ) result |= 0x0000000000008000;
    if (v->IsInt32Array()       ) result |= 0x0000000000010000;
    if (v->IsInt32()            ) result |= 0x0000000000020000;
    if (v->IsInt8Array()        ) result |= 0x0000000000040000;
    if (v->IsMapIterator()      ) result |= 0x0000000000080000;
    if (v->IsMap()              ) result |= 0x0000000000100000;
    if (v->IsName()             ) result |= 0x0000000000200000;
    if (v->IsNativeError()      ) result |= 0x0000000000400000;
    if (v->IsNull()             ) result |= 0x0000000000800000;
    if (v->IsNumberObject()     ) result |= 0x0000000001000000;
    if (v->IsNumber()           ) result |= 0x0000000002000000;
    if (v->IsObject()           ) result |= 0x0000000004000000;
    if (v->IsPromise()          ) result |= 0x0000000008000000;
    if (v->IsRegExp()           ) result |= 0x0000000010000000;
    if (v->IsSetIterator()      ) result |= 0x0000000020000000;
    if (v->IsSet()              ) result |= 0x0000000040000000;
    if (v->IsStringObject()     ) result |= 0x0000000080000000;
    if (v->IsString()           ) result |= 0x0000000100000000;
    if (v->IsSymbolObject()     ) result |= 0x0000000200000000;
    if (v->IsSymbol()           ) result |= 0x0000000400000000;
    if (v->IsTrue()             ) result |= 0x0000000800000000;
    if (v->IsTypedArray()       ) result |= 0x0000001000000000;
    if (v->IsUint16Array()      ) result |= 0x0000002000000000;
    if (v->IsUint32Array()      ) result |= 0x0000004000000000;
    if (v->IsUint32()           ) result |= 0x0000008000000000;
    if (v->IsUint8Array()       ) result |= 0x0000010000000000;
    if (v->IsUint8ClampedArray()) result |= 0x0000020000000000;
    if (v->IsUndefined()        ) result |= 0x0000040000000000;
    if (v->IsWeakMap()          ) result |= 0x0000080000000000;
    if (v->IsWeakSet()          ) result |= 0x0000100000000000;
    return result;
}
