#include "Arduino.h"
#include "../../core/options.h"
#if L10N_LANGUAGE == PL
#include "../dspcore.h"

// Polish chars: ąćęłńóśźż ĄĆĘŁŃÓŚŹŻ
// Using custom CGRAM characters (slots 0-7)

char* utf8Rus(const char* str, bool uppercase) {
  static char strn[BUFLEN];
  int inIndex = 0;
  int outIndex = 0;
  
  while (str[inIndex] && outIndex < BUFLEN - 1) {
    uint8_t c = (uint8_t)str[inIndex];
    
    // Handle UTF-8 multibyte sequences
    if (c == 0xC4) {  // UTF-8 first byte 0xC4
      inIndex++;
      uint8_t c2 = (uint8_t)str[inIndex];
      switch (c2) {
        case 0x85: strn[outIndex++] = uppercase ? 'A' : 0x00; break; // ą/Ą -> custom char 0
        case 0x84: strn[outIndex++] = 'A'; break; // Ą -> A
        case 0x87: strn[outIndex++] = uppercase ? 'C' : 0x01; break; // ć/Ć -> custom char 1
        case 0x86: strn[outIndex++] = 'C'; break; // Ć -> C
        case 0x99: strn[outIndex++] = uppercase ? 'E' : 0x02; break; // ę/Ę -> custom char 2
        case 0x98: strn[outIndex++] = 'E'; break; // Ę -> E
        default: strn[outIndex++] = '?'; break;
      }
      inIndex++;
    }
    else if (c == 0xC5) {  // UTF-8 first byte 0xC5
      inIndex++;
      uint8_t c2 = (uint8_t)str[inIndex];
      switch (c2) {
        case 0x82: strn[outIndex++] = uppercase ? 'L' : 0x03; break; // ł/Ł -> custom char 3
        case 0x81: strn[outIndex++] = 'L'; break; // Ł -> L
        case 0x84: strn[outIndex++] = uppercase ? 'N' : 0x04; break; // ń/Ń -> custom char 4
        case 0x83: strn[outIndex++] = 'N'; break; // Ń -> N
        case 0x9B: strn[outIndex++] = uppercase ? 'S' : 0x06; break; // ś/Ś -> custom char 6
        case 0x9A: strn[outIndex++] = 'S'; break; // Ś -> S
        case 0xBA: strn[outIndex++] = uppercase ? 'Z' : 'z'; break; // ź/Ź -> z/Z (use regular z)
        case 0xB9: strn[outIndex++] = 'Z'; break; // Ź -> Z
        case 0xBC: strn[outIndex++] = uppercase ? 'Z' : 0x07; break; // ż/Ż -> custom char 7
        case 0xBB: strn[outIndex++] = 'Z'; break; // Ż -> Z
        default: strn[outIndex++] = '?'; break;
      }
      inIndex++;
    }
    else if (c == 0xC3) {  // UTF-8 first byte 0xC3
      inIndex++;
      uint8_t c2 = (uint8_t)str[inIndex];
      switch (c2) {
        case 0xB3: strn[outIndex++] = uppercase ? 'O' : 0x05; break; // ó/Ó -> custom char 5
        case 0x93: strn[outIndex++] = 'O'; break; // Ó -> O
        default: strn[outIndex++] = '?'; break;
      }
      inIndex++;
    }
    else {
      // Regular ASCII character
      strn[outIndex++] = uppercase ? toupper(c) : c;
      inIndex++;
    }
  }
  
  strn[outIndex] = 0;
  return strn;
}

#endif //#if L10N_LANGUAGE == PL