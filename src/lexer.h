#ifndef LEXER
#define LEXER

#include "./datatype.h"
#include <string>
#include <vector>

enum Token {
    // Operators CURRENTLY WRONG
    op_eq = 15677,
    op_or = 31868,
    op_neq = 15649,
    op_geq = 15678,
    op_leq = 15676,
    op_shl = 15420,
    op_shr = 15934,

    tok_eof = -1,

    //commands
    tok_def = -2,
    tok_extern = -3,

    //primary
    tok_identifier = -4,
    tok_number = -5,
    tok_true = -10,
    tok_false = -11,

    // control
    tok_if = -12,
    tok_else = -13,
    tok_for = -14,

    //operators
    tok_binary = -15,
    tok_unary = -16,

    // var definition
    tok_dtype = -17,
};

int optok(std::string op);
std::string tokop(int op);

extern std::string IdentifierStr; //Filled in if tok_identifier
extern double NumVal;             //Filled in if tok_number
extern int64_t INumVal;             //Filled in if tok_number
extern DataType TokenDataType;

int gettok();

extern int CurTok;
int getNextToken();
#endif
