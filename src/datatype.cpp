#include "datatype.h"
#include "./BinopsData.h"
#include <algorithm>
#include <iterator>

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
        case type_u8: 
            return "uint8";
        case type_u16:
            return "uint16";
        case type_u32:
            return "uint32";
        case type_u64:
            return "uint64";
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
            return 'u'; // undecided
        case type_bool:
            return 'b'; // bool
        case type_i8: 
            return 'C'; // char
        case type_i16:
            return 'S'; // short
        case type_i32:
            return 'I'; // int  
        case type_i64:
            return 'L'; // long
        case type_u8: 
            return 'c'; // char (lowercase to denote sign)
        case type_u16:
            return 's'; // short
        case type_u32:
            return 'i'; // int 
        case type_u64:
            return 'l'; // long
        case type_float:
            return 'f'; // float
        case type_double:
            return 'd'; // double
        case type_void:
            return 'v'; // void
        default:
            return 'U'; // unknown
    }
}

bool isSigned(DataType dtype){
    if (dtype == type_bool || dtype == type_u8 || dtype == type_u16 ||
            dtype == type_u32 || dtype == type_u64) { return false; }
    else { return true; }
}
bool isFP(DataType dtype){
    if (dtype == type_double || dtype == type_float) { return true; }
    else { return false; }
}

unsigned getIndex(DataType dtype){
    return std::distance(priorities, std::find(priorities, priorities + numPriorities, dtype));
}
bool canExpandToDouble(DataType dtype){
    return getIndex(dtype) > getIndex(type_double);
}

DataType getExpandType(DataType left, DataType right) {
    DataType biggerType = type_UNDECIDED;
    DataType smallerType = type_UNDECIDED;
    for(int i = 0; i <= numPriorities; i++){
        if (left == priorities[i] || right == priorities[i]){
            if(biggerType == type_UNDECIDED)
                biggerType = priorities[i];
            smallerType = priorities[i];
        }
    }

    // No need for an expansion if both sides are the same already
    if (biggerType == smallerType)
        return biggerType;

    // If the smaller type is floating point, but the larger type is not
    // there is a risk of data loss, if both can not be converted to a double.
    if (isFP(smallerType) && !isFP(biggerType)) {
        if (canExpandToDouble(biggerType)){
            return type_double;
        }
        return type_UNDECIDED;
    }

    //  If the larger value is an unsigned int, while the smaller one is signed
    //  It must expand to a larger signed int type. u8 and i8 expand to i16
    if (!isFP(smallerType) && !isFP(biggerType) && isSigned(smallerType) && !isSigned(biggerType)) {
        switch (biggerType){
            case type_u8:
                return type_i16;
            case type_u16:
                return type_i32;
            case type_u32:
                return type_i64;
            // if it is already 64 bits, there is no possible expansion.
            // Data loss may occur
            default:
                return type_UNDECIDED;
        }
    }

    return biggerType;
}

