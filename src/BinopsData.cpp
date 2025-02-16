#include "./BinopsData.h"
#include "./lexer.h"
#include "datatype.h"

std::vector<int> longops;
std::map<int, BinopProperty> BinopProperties;
std::map<int, std::map<DataType, DataType>> UnopProperties;
const DataType priorities[] = {type_i64, type_double,type_i32, type_float,type_i16, type_i8, type_bool};
const int numPriorities = 6;

void InitializeBinopPrecedence() {
    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopProperties['='] =
        {10, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['|'] =
        {20, std::map<std::pair<DataType, DataType>, DataType>()}; 
    BinopProperties[optok("||")] = 
        {20, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['&'] =
        {30, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['>'] =
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['<'] =
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties[optok("==")] = 
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties[optok("!=")] = 
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties[optok(">=")] =
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties[optok("<=")] =
        {40, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['+'] =
        {50, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['-'] =
        {50, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['*'] =
        {60, std::map<std::pair<DataType, DataType>, DataType>()};
    BinopProperties['/'] =
        {60, std::map<std::pair<DataType, DataType>, DataType>()};

    for(int i = 0; i <= numPriorities; i++){
        DataType Bigger = priorities[i];
        for (int j = i; j <= numPriorities; j++){
            DataType Smaller = priorities[j];
            std::pair<DataType, DataType> pair1 = std::make_pair(Bigger, Smaller);
            std::pair<DataType, DataType> pair2 = std::make_pair(Smaller, Bigger);
            BinopProperties['='].CompatibilityChart[pair1] = Bigger;

            if(!(Bigger == type_double || Bigger == type_float 
                    || Smaller == type_double || Smaller == type_float)) {
                // If not a floating point operator, install operators
                BinopProperties['|'].CompatibilityChart[pair1] = Bigger;
                BinopProperties['|'].CompatibilityChart[pair2] = Bigger;
                BinopProperties[optok("||")].CompatibilityChart[pair1] = Bigger;
                BinopProperties[optok("||")].CompatibilityChart[pair2] = Bigger;
                BinopProperties['&'].CompatibilityChart[pair1] = Bigger;
                BinopProperties['&'].CompatibilityChart[pair2] = Bigger;
            } else {
                // Otherwise, skip all operations that will result in data loss.
                if (Smaller == type_double && j < i)
                    continue;
                else if (Smaller == type_float && j < i && Bigger != type_double)
                    continue;
            }

            BinopProperties[optok("==")].CompatibilityChart[pair1] = type_bool;
            BinopProperties[optok("==")].CompatibilityChart[pair2] = type_bool;
            BinopProperties[optok("!=")].CompatibilityChart[pair1] = type_bool;
            BinopProperties[optok("!=")].CompatibilityChart[pair2] = type_bool;
            BinopProperties['+'].CompatibilityChart[pair1] = Bigger;
            BinopProperties['+'].CompatibilityChart[pair2] = Bigger;
            BinopProperties['-'].CompatibilityChart[pair1] = Bigger;
            BinopProperties['-'].CompatibilityChart[pair2] = Bigger;
            BinopProperties['*'].CompatibilityChart[pair1] = Bigger;
            BinopProperties['*'].CompatibilityChart[pair2] = Bigger;
            BinopProperties['/'].CompatibilityChart[pair1] = Bigger;
            BinopProperties['/'].CompatibilityChart[pair2] = Bigger;
            // Do not check for bools.
            if (i != numPriorities) {
                BinopProperties['>'].CompatibilityChart[pair1] = type_bool;
                BinopProperties['>'].CompatibilityChart[pair2] = type_bool;
                BinopProperties['<'].CompatibilityChart[pair1] = type_bool;
                BinopProperties['<'].CompatibilityChart[pair2] = type_bool;
                BinopProperties[optok(">=")].CompatibilityChart[pair1] = type_bool;
                BinopProperties[optok(">=")].CompatibilityChart[pair2] = type_bool;
                BinopProperties[optok("<=")].CompatibilityChart[pair1] = type_bool;
                BinopProperties[optok("<=")].CompatibilityChart[pair2] = type_bool;
            }
        }
    }
    std::pair<DataType, DataType> boolop = std::make_pair(type_bool, type_bool);

    longops.push_back(optok("||"));
    longops.push_back(optok("=="));
    longops.push_back(optok("!="));
    longops.push_back(optok(">="));
    longops.push_back(optok("<="));

    UnopProperties['!'] = std::map<DataType, DataType>();
    UnopProperties['-'] = std::map<DataType, DataType>();
    UnopProperties['!'][type_bool] = type_bool;
    for(int i = 0; i <= numPriorities; i++){
        if (priorities[i] != type_bool){
            UnopProperties['-'][priorities[i]] = priorities[i];
        }
    }
}
