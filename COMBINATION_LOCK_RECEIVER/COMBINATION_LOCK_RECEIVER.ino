#include <WiFi.h>
#include <esp_now.h>
#include "TFT_GC9D01N.h"

#define ESPNOW_WIFI_CHANNEL 6

// =========================================================
// 16×32 FONT STORAGE — ONLY LETTERS FOR "UNLOCKED"
// =========================================================
uint8_t font16x32_letters[96][64] = {0};

// ---- 16×32 bitmaps for each needed letter ----

const uint8_t LETTER_U[64] = {
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x38, 0x38, 0x3C, 0x78, 0x1F, 0xF0, 0x0F, 0xE0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};



const uint8_t LETTER_N[64] = {
  0x70, 0x1C, 0x78, 0x1C, 0x7C, 0x1C, 0x76, 0x1C,
  0x73, 0x1C, 0x71, 0x9C, 0x70, 0xDC, 0x70, 0x7C,
  0x70, 0x3C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t LETTER_L[64] = {
  0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00,
  0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00,
  0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00,
  0x7F, 0xF8, 0x7F, 0xF8, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t LETTER_O[64] = {
  0x0F, 0xE0, 0x1F, 0xF0, 0x3C, 0x78, 0x38, 0x38,
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x70, 0x1C, 0x70, 0x1C, 0x38, 0x38, 0x3C, 0x78,
  0x1F, 0xF0, 0x0F, 0xE0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t LETTER_C[64] = {
  0x0F, 0xE0, 0x1F, 0xF0, 0x3C, 0x78, 0x38, 0x38,
  0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00,
  0x70, 0x00, 0x38, 0x38, 0x3C, 0x78, 0x1F, 0xF0,
  0x0F, 0xE0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0
};

const uint8_t LETTER_K[64] = {
  0x70, 0x1C, 0x70, 0x38, 0x70, 0x70, 0x70, 0xE0,
  0x71, 0xC0, 0x73, 0x80, 0x77, 0x00, 0x7F, 0x00,
  0x77, 0x80, 0x73, 0xC0, 0x71, 0xE0, 0x70, 0xF0,
  0x70, 0x78, 0x70, 0x3C, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t LETTER_E[64] = {
  0x7F, 0xF8, 0x7F, 0xF8, 0x70, 0x00, 0x70, 0x00,
  0x70, 0x00, 0x7F, 0xE0, 0x7F, 0xE0, 0x70, 0x00,
  0x70, 0x00, 0x70, 0x00, 0x7F, 0xF8, 0x7F, 0xF8,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t LETTER_D[64] = {
  0x7F, 0xE0, 0x7F, 0xF0, 0x70, 0x78, 0x70, 0x3C,
  0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C, 0x70, 0x1C,
  0x70, 0x3C, 0x70, 0x78, 0x7F, 0xF0, 0x7F, 0xE0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// =========================================================
// Map only the letters we use for UNLOCKED
// =========================================================
int mapLetterIndex(char c) {
  switch (c) {
    case 'U': return 30;
    case 'N': return 23;
    case 'L': return 21;
    case 'O': return 24;
    case 'C': return 12;
    case 'K': return 20;
    case 'E': return 14;
    case 'D': return 13;
  }
  return -1;
}

// =========================================================
// Load letters into their positions
// =========================================================
void load16x32Letters() {
  memcpy(font16x32_letters[30], LETTER_U, 64);
  memcpy(font16x32_letters[23], LETTER_N, 64);
  memcpy(font16x32_letters[21], LETTER_L, 64);
  memcpy(font16x32_letters[24], LETTER_O, 64);
  memcpy(font16x32_letters[12], LETTER_C, 64);
  memcpy(font16x32_letters[20], LETTER_K, 64);
  memcpy(font16x32_letters[14], LETTER_E, 64);
  memcpy(font16x32_letters[13], LETTER_D, 64);
}

// =========================================================
// Draw ONE 16×32 character
// =========================================================
void DispOneChar32(TFT_GC9D01N_Class &tft,
                   char ord,
                   unsigned int Xstart,
                   unsigned int Ystart,
                   unsigned int TextColor,
                   unsigned int BackColor)
{
  int idx = mapLetterIndex(ord);

  if (idx < 0) {
    tft.DispOneChar(ord, Xstart, Ystart, TextColor, BackColor);
    return;
  }

  const uint8_t* p = font16x32_letters[idx];

#if LANDSCAPE
  tft.BlockWrite(Xstart, Xstart + 15, Ystart, Ystart + 31);

  for (int row = 0; row < 32; row++) {
    uint16_t line = (p[row * 2] << 8) | p[row * 2 + 1];
    for (int col = 0; col < 16; col++) {
      bool pixel = line & (1 << (15 - col));
      tft.WriteOneDot(pixel ? TextColor : BackColor);
    }
  }

#else
  tft.BlockWrite(Xstart, Xstart + 15, Ystart, Ystart + 31);

  for (int row = 0; row < 32; row++) {
    uint16_t line = (p[row * 2] << 8) | p[row * 2 + 1];
    for (int col = 0; col < 16; col++) {
      bool pixel = line & (1 << (15 - col));
      tft.WriteOneDot(pixel ? TextColor : BackColor);
    }
  }
#endif
}

// =========================================================
// Draw a string using 16×32 where available
// =========================================================
void DispStr32(TFT_GC9D01N_Class &tft,
               const char *str,
               unsigned int Xstart,
               unsigned int Ystart,
               unsigned int TextColor,
               unsigned int BackColor)
{
  while (*str) {
    char c = *str;
    int idx = mapLetterIndex(c);

#if LANDSCAPE
    if (idx >= 0) {
      DispOneChar32(tft, c, Xstart, Ystart, TextColor, BackColor);
      Ystart += 16;
    } else {
      tft.DispOneChar(c, Xstart, Ystart, TextColor, BackColor);
      Ystart += FONT_W;
    }
#else
    if (idx >= 0) {
      DispOneChar32(tft, c, Xstart, Ystart, TextColor, BackColor);
      Xstart += 16;
    } else {
      tft.DispOneChar(c, Xstart, Ystart, TextColor, BackColor);
      Xstart += FONT_W;
    }
#endif

    str++;
  }
}

// =========================================================
// ESP-NOW STRUCT
// =========================================================
typedef struct struct_message {
  char command[20];
  bool unlocked;
  uint8_t code[4];
  uint8_t step;
  uint8_t total_steps;
} struct_message;

struct_message receivedData;
TFT_GC9D01N_Class tft;

// =========================================================
// STATE MANAGEMENT
// =========================================================
bool isLocked = true;
bool displayChanged = false;

// =========================================================
// Display current state
// =========================================================
void displayLockState() {
  tft.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
  
  if (isLocked) {
    DispStr32(tft, "LOCKED", 14, 30, RED, BLACK);
  } else {
    DispStr32(tft, "UNLOCKED", 14, 20, GREEN, BLACK);
  }
  
  displayChanged = false;
}

// =========================================================
// ESP-NOW CALLBACK
// =========================================================
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len >= sizeof(receivedData)) {
    memcpy(&receivedData, data, sizeof(receivedData));
    
    bool oldState = isLocked;
    
    if (strcmp(receivedData.command, "UNLOCK") == 0 && receivedData.unlocked) {
      isLocked = false;
      displayChanged = true;
    }
    else if (strcmp(receivedData.command, "RESET") == 0) {
      isLocked = true;
      displayChanged = true;
    }
    
    if (oldState != isLocked && displayChanged) {
      displayLockState();
    }
  }
}

// =========================================================
// Setup
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  load16x32Letters();

  tft.begin();
  tft.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
  
  isLocked = true;
  displayLockState();

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

// =========================================================
// Main loop
// =========================================================
void loop() {
  delay(100);
}
