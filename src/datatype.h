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
    type_u8 = 5,
    type_u16 = 6,
    type_u32 = 7,
    type_u64 = 8,
    type_float = 9,
    type_double = 10,
    type_void = 11,
};

std::string dtypeToString(DataType dtype);
char dtypeToChar(DataType dtype);

bool isFP(DataType dtype);
bool isSigned(DataType dtype);
DataType getExpandType(DataType left, DataType right);

#endif
