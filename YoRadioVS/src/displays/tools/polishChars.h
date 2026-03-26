#ifndef polishChars_h
#define polishChars_h

#if L10N_LANGUAGE == PL

// Polish character bitmaps (5x8 pixels)
// LCD custom char slots: 0-7 (we have 8 slots available)

// Lowercase Polish characters
const uint8_t char_a_ogonek[8] = {  // ą (slot 0)
  0b01110,
  0b00001,
  0b01111,
  0b10001,
  0b01111,
  0b00010,
  0b00100,
  0b00000
};

const uint8_t char_c_acute[8] = {   // ć (slot 1)
  0b00100,
  0b01000,
  0b01110,
  0b10000,
  0b10000,
  0b10001,
  0b01110,
  0b00000
};

const uint8_t char_e_ogonek[8] = {  // ę (slot 2)
  0b01110,
  0b10001,
  0b11111,
  0b10000,
  0b01110,
  0b00010,
  0b00100,
  0b00000
};

const uint8_t char_l_stroke[8] = {  // ł (slot 3)
  0b01100,
  0b00100,
  0b01110,
  0b00100,
  0b00100,
  0b00100,
  0b01110,
  0b00000
};

const uint8_t char_n_acute[8] = {   // ń (slot 4)
  0b00100,
  0b01000,
  0b10110,
  0b11001,
  0b10001,
  0b10001,
  0b10001,
  0b00000
};

const uint8_t char_o_acute[8] = {   // ó (slot 5)
  0b00100,
  0b01000,
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b01110,
  0b00000
};

const uint8_t char_s_acute[8] = {   // ś (slot 6)
  0b00100,
  0b01000,
  0b01111,
  0b10000,
  0b01110,
  0b00001,
  0b11110,
  0b00000
};

const uint8_t char_z_dot[8] = {     // ż (slot 7)
  0b00100,
  0b00000,
  0b11111,
  0b00010,
  0b00100,
  0b01000,
  0b11111,
  0b00000
};

#endif // L10N_LANGUAGE == PL
#endif // polishChars_h