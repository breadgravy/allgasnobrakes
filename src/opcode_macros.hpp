
#ifdef DECL_ENUM
    #undef OPCODE
    #define OPCODE(__name__) __name__,
#elif defined(DECL_STRING_TABLE)
    #undef OPCODE
    #define OPCODE(__name__) #__name__,
#else 
    #undef OPCODE
    #define OPCODE(__name__) 
#endif

