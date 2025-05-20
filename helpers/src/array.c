#include "string.h"

char getHexEscapeCaracter(int value)
{
    switch (value) {
        case 1:
           return '\x1';
        case 2:
           return '\x2';
        case 3:
           return '\x3';
        case 4:
           return '\x4';
        case 5:
           return '\x5';
        case 6:
           return '\x6';
        case 7:
           return '\x7';
        case 8:
           return '\x8';
        case 9:
           return '\x9';
        default:
           return '\x0';
    }
}

void strCopyWithSize(char *destination, char *source) {   
   destination[0] = strlen(source);
   strcat(destination, source);
}