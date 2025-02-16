#ifndef BINOP
#define BINOP
#include "./datatype.h"
#include <map>
#include <vector>

/// BinopProperties - This holds the precedence for each binary operator that is
/// defined.
struct BinopProperty{
    int Precedence;
    std::map<std::pair<DataType, DataType>, DataType> CompatibilityChart;
};

extern std::map<int, BinopProperty> BinopProperties;
extern std::map<int, std::map<DataType, DataType>> UnopProperties;
extern std::vector<int> longops;
extern const DataType priorities[];
extern const int numPriorities;

void InitializeBinopPrecedence();

#endif
