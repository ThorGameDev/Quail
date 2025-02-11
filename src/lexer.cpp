#include "./lexer.h"
#include "./BinopsData.h"
#include "datatype.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

// The lexer returns tokens [0-255] if it is an unknown character, otherwise
// one of these for known things. It returns tokens greater than 255 for
// multi-part operators

std::string IdentifierStr; //Filled in if tok_identifier
double NumVal;             //Filled in if tok_number
int64_t INumVal;             //Filled in if tok_number
DataType TokenDataType;

int optok(std::string op) {
    return (op[1] << 8) + op[0];
}

std::string tokop(int op) {
    std::string ret;
    ret += (char)(op);
    char second = op >> 8;
    if (second != 0) {
        ret += second;
    }
    return ret;
}

// gettok - Return the next token from the standard input.
int gettok() {
    static char LastChar = ' ';

    //Skip any white space
    while (isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "binary")
            return tok_binary;
        if (IdentifierStr == "unary")
            return tok_unary;
        if (IdentifierStr == "double"){
            TokenDataType = type_double;
            return tok_dtype;
        }
        if (IdentifierStr == "float"){
            TokenDataType = type_float;
            return tok_dtype;
        }
        if (IdentifierStr == "bool"){
            TokenDataType = type_bool;
            return tok_dtype;
        }
        if (IdentifierStr == "i64"){
            TokenDataType = type_i64;
            return tok_dtype;
        }
        if (IdentifierStr == "i32"){
            TokenDataType = type_i32;
            return tok_dtype;
        }
        if (IdentifierStr == "i16"){
            TokenDataType = type_i16;
            return tok_dtype;
        }
        if (IdentifierStr == "i8"){
            TokenDataType = type_i8;
            return tok_dtype;
        }
        if (IdentifierStr == "true")
            return tok_true;
        if (IdentifierStr == "false")
            return tok_false;

        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.') {
        std::string NumStr;
        std::string INumStr;
        bool isInt = true;
        do {
            if (LastChar == '.')
                isInt = false;
            if (isInt)
                INumStr += LastChar;
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');


        NumVal = strtod(NumStr.c_str(), 0);
        INumVal = strtoll(INumStr.c_str(), 0, 10);
        if (LastChar == ':'){
            LastChar = getchar();
            std::string ExplicitType;
            do {
                ExplicitType += LastChar;
                LastChar = getchar();
            } 
            while(isdigit(LastChar) || LastChar == 'i' || LastChar == 'f' || LastChar == 'd');
            if (ExplicitType == "i64")
                TokenDataType = type_i64;
            if (ExplicitType == "i32")
                TokenDataType = type_i32;
            if (ExplicitType == "i16")
                TokenDataType = type_i16;
            if (ExplicitType == "i8")
                TokenDataType = type_i8;
            if (ExplicitType == "d")
                TokenDataType = type_double;
            if (ExplicitType == "f")
                TokenDataType = type_float;
        }
        else {
            if (isInt){
                if ((int8_t)INumVal == INumVal){
                    TokenDataType = type_i8; 
                } else if ((int16_t)INumVal == INumVal) {
                    TokenDataType = type_i16; 
                } else if ((int32_t)INumVal == INumVal) {
                    TokenDataType = type_i32; 
                } else {
                    TokenDataType = type_i64; 
                }
            }
            else {
                if ((float)NumVal == NumVal){
                    TokenDataType = type_float;
                } else {
                    TokenDataType = type_double;
                }
                
            }
        }
        return tok_number;
    }

    if (LastChar == '#') {
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    // Check for end of file
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    char ThisChar = LastChar;
    LastChar = getchar();

    // But first, check to make sure it isnt a multipart operator
    int value = (LastChar << 8) + ThisChar;
    for (int val = 0; val < longops.size(); val++) {
        if (longops[val] == value) {
            LastChar = getchar();
            return value;
        }
    }

    return ThisChar;
}

int CurTok;
int getNextToken() {
    return CurTok = gettok();
}
