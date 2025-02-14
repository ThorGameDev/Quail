#ifndef DATATYPE
#define DATATYPE

#include <string>
enum DataType {
    type_UNDECIDED = 999,
    type_bool = 0,
    type_i8 = 1,
    type_i16 = 2,
    type_i32 = 3,
    type_i64 = 4,
    type_float = 5,
    type_double = 6,
    type_void = 7,
};

std::string dtypeToString(DataType dtype);
char dtypeToChar(DataType dtype);

#endif
