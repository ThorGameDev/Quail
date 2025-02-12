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
        default:
            return "Unknown";
    }
}
