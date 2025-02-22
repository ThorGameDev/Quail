#include "./lexer.h"
#include "./BinopsData.h"
#include "datatype.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

std::string fileData;
int index = 0;
bool jitMode;

void initBuffer() {
    jitMode = true;
    index = 0;
    std::getline(std::cin, fileData);
    getNextToken();
}

void readFile(char* filepath) {
    jitMode = false;
    index = 0;
    std::ifstream file(filepath);
    std::string str;
    while (std::getline(file, str)) {
        fileData += str; 
        fileData.push_back('\n'); 
    }
    getNextToken();
}

char nextChar() {
    if (index >= fileData.size()) {
        return EOF;
    }
    return fileData[index++];
}


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
    if (op <= 0){
        switch (op) {
            case tok_def:
                return "tok_def";
            case tok_extern:
                return "tok_extern";
            case tok_identifier:
                return "tok_identifier";
            case tok_number:
                return "tok_number";
            case tok_true:
                return "tok_true";
            case tok_false:
                return "tok_false";
            case tok_if:
                return "tok_if";
            case tok_else:
                return "tok_else";
            case tok_for:
                return "tok_for";
            case tok_operator:
                return "tok_operator";
            case tok_dtype:
                return "tok_dtype";
            default:
                return std::to_string(op);
        }
    }
    std::string ret;
    ret += (char)(op);
    char second = op >> 8;
    if (second != 0) {
        ret += second;
    }
    return ret;
}

// gettok - Return the next token from the standard input.
static char LastChar = ' ';
int gettok() {
    //Skip any white space
    while (isspace(LastChar))
        LastChar = nextChar();

    if (isalpha(LastChar) || LastChar == '_') {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = nextChar())) || LastChar == '_')
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        else if (IdentifierStr == "extern")
            return tok_extern;
        else if (IdentifierStr == "if")
            return tok_if;
        else if (IdentifierStr == "else")
            return tok_else;
        else if (IdentifierStr == "for")
            return tok_for;
        else if (IdentifierStr == "operator")
            return tok_operator;
        else if (IdentifierStr == "double"){
            TokenDataType = type_double;
            return tok_dtype;
        }
        else if (IdentifierStr == "float"){
            TokenDataType = type_float;
            return tok_dtype;
        }
        else if (IdentifierStr == "bool"){
            TokenDataType = type_bool;
            return tok_dtype;
        }
        else if (IdentifierStr == "i64"){
            TokenDataType = type_i64;
            return tok_dtype;
        }
        else if (IdentifierStr == "i32"){
            TokenDataType = type_i32;
            return tok_dtype;
        }
        else if (IdentifierStr == "i16"){
            TokenDataType = type_i16;
            return tok_dtype;
        }
        else if (IdentifierStr == "i8"){
            TokenDataType = type_i8;
            return tok_dtype;
        }
        else if (IdentifierStr == "u64"){
            TokenDataType = type_u64;
            return tok_dtype;
        }
        else if (IdentifierStr == "u32"){
            TokenDataType = type_u32;
            return tok_dtype;
        }
        else if (IdentifierStr == "u16"){
            TokenDataType = type_u16;
            return tok_dtype;
        }
        else if (IdentifierStr == "u8"){
            TokenDataType = type_u8;
            return tok_dtype;
        }
        else if (IdentifierStr == "void"){
            TokenDataType = type_void;
            return tok_dtype;
        }
        else if (IdentifierStr == "true")
            return tok_true;
        else if (IdentifierStr == "false")
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
            LastChar = nextChar();
        } while (isdigit(LastChar) || LastChar == '.');


        NumVal = strtod(NumStr.c_str(), 0);
        INumVal = strtoll(INumStr.c_str(), 0, 10);
        if (LastChar == ':'){
            LastChar = nextChar();
            std::string ExplicitType;
            do {
                ExplicitType += LastChar;
                LastChar = nextChar();
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
            if (ExplicitType == "u64")
                TokenDataType = type_u64;
            if (ExplicitType == "u32")
                TokenDataType = type_u32;
            if (ExplicitType == "u16")
                TokenDataType = type_u16;
            if (ExplicitType == "u8")
                TokenDataType = type_u8;
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
            LastChar = nextChar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    // Check for end of file
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    char ThisChar = LastChar;
    LastChar = nextChar();

    // But first, check to make sure it isnt a multipart operator
    int value = (LastChar << 8) + ThisChar;
    for (int val = 0; val < longops.size(); val++) {
        if (longops[val] == value) {
            LastChar = nextChar();
            return value;
        }
    }

    return ThisChar;
}

int CurTok;
int getNextToken() {
    return CurTok = gettok();
}

void resetLexer() {
    fileData = "";
    LastChar = ' ';
    CurTok = ' ';
    IdentifierStr = "";
    NumVal = 0;
    INumVal = 0;
    TokenDataType = type_void;
}
