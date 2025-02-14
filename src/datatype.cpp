#include "datatype.h"

std::string dtypeToString(DataType dtype) {
    switch (dtype){
        case type_UNDECIDED:
            return "undecided";
        case type_bool:
            return "bool";
        case type_i8: 
            return "int8";
        case type_i16:
            return "int16";
        case type_i32:
            return "int32";
        case type_i64:
            return "int64";
        case type_float:
            return "float";
        case type_double:
            return "double";
        case type_void:
            return "void";
        default:
            return "Unknown";
    }
}

// For name mangling. User has no reason to see
char dtypeToChar(DataType dtype) {
    switch (dtype){
        case type_UNDECIDED:
            return 'u';
        case type_bool:
            return 'b';
        case type_i8: 
            return '3'; // 2^n bits
        case type_i16:
            return '4';
        case type_i32:
            return '5';
        case type_i64:
            return '6';
        case type_float:
            return 'f';
        case type_double:
            return 'd';
        case type_void:
            return 'v';
        default:
            return 'U';
    }
}
