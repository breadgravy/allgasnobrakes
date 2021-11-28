#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"

#ifdef DECL_ENUM
    #define OPCODE(__name__) __name__,
#elif defined(DECL_STRING_TABLE)
    #define OPCODE(__name__) #__name__,
#else 
    #define OPCODE(__name__) 
#endif

#pragma GCC diagnostic pop
