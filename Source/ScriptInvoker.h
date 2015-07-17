#ifndef __SCRIPT_INVOKER__
#define __SCRIPT_INVOKER__

#include "Common.h"

struct DeferredCall
{
    uint   Id;
    uint   FireTick;
    hash   FuncNum;
    uint   BindId;
    bool   IsValue;
    int    Value;
    bool   IsValues;
    bool   ValuesSigned;
    IntVec Values;
    bool   Saved;
};
typedef list< DeferredCall > DeferredCallList;

class ScriptInvoker
{
    friend class Script;

private:
    uint             lastDeferredCallId;
    DeferredCallList deferredCalls;
    Mutex            deferredCallsLocker;

    ScriptInvoker();
    uint   AddDeferredCall( uint delay, bool saved, asIScriptFunction* func, int* value, ScriptArray* values );
    bool   IsDeferredCallPending( uint id );
    bool   CancelDeferredCall( uint id );
    bool   GetDeferredCallData( uint id, DeferredCall& data );
    void   GetDeferredCallsList( IntVec& ids );
    void   Process();
    void   RunDeferredCall( DeferredCall& call );
    void   SaveDeferredCalls( void ( * save_func )( void*, size_t ) );
    bool   LoadDeferredCalls( void* f, uint version );
    string GetStatistics();

public:
    static uint Global_DeferredCall( uint delay, asIScriptFunction* func );
    static uint Global_DeferredCallWithValue( uint delay, asIScriptFunction* func, int value );
    static uint Global_DeferredCallWithValues( uint delay, asIScriptFunction* func, ScriptArray* values );
    static uint Global_SavedDeferredCall( uint delay, asIScriptFunction* func );
    static uint Global_SavedDeferredCallWithValue( uint delay, asIScriptFunction* func, int value );
    static uint Global_SavedDeferredCallWithValues( uint delay, asIScriptFunction* func, ScriptArray* values );
    static bool Global_IsDeferredCallPending( uint id );
    static bool Global_CancelDeferredCall( uint id );
    static bool Global_GetDeferredCallData( uint id, uint& delay, ScriptArray* values );
    static uint Global_GetDeferredCallsList( ScriptArray* ids );
};

#endif // __SCRIPT_INVOKER__