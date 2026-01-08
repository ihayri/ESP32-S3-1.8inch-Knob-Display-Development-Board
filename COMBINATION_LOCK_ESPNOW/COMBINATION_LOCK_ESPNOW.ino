// ===== FILE: combo_lock_esp32s3_fixed.ino =====
#include "lcd_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "lcd_config.h"
#include <Arduino.h>
#include "driver/i2c.h"
#include "bidi_switch_knob.h"
#include "ESP32_NOW.h"
#include "WiFi.h"

// ===== ESPNOW SETTINGS =====
#define ESPNOW_WIFI_CHANNEL 6

// Create broadcast peer class
class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) : 
    ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  ~ESP_NOW_Broadcast_Peer() {
    remove();
  }

  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      Serial.println("Failed to initialize ESP-NOW or register broadcast peer");
      return false;
    }
    return true;
  }

  bool send_message(const uint8_t *data, size_t len) {
    if (!send(data, len)) {
      Serial.println("Failed to broadcast message");
      return false;
    }
    return true;
  }
};

ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, nullptr);

// Message structure
typedef struct struct_message {
    char command[20];
    bool unlocked;
    uint8_t code[4];
    uint8_t step;
    uint8_t total_steps;
} struct_message;

struct_message espnowData;

// ===== HAPTIC PINS =====
#define HAPTIC_SDA  11
#define HAPTIC_SCL  12
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// ===== ENCODER PINS =====
#define EXAMPLE_ENCODER_ECA_PIN    8
#define EXAMPLE_ENCODER_ECB_PIN    7

// ===== HAPTIC EFFECTS =====
#define HAPTIC_TICK 1
#define HAPTIC_CLICK 4
#define HAPTIC_CORRECT 12
#define HAPTIC_UNLOCK 14
#define HAPTIC_WRONG 15
#define HAPTIC_RESET 118
#define HAPTIC_START 27

// ===== COMBO LOCK SETTINGS =====
#define CODE_LENGTH 4
uint8_t secret_code[CODE_LENGTH] = {25, 10, 35, 5};
bool secret_directions[CODE_LENGTH] = {true, false, true, false};
uint8_t entered_code[CODE_LENGTH] = {0};
bool entered_directions[CODE_LENGTH] = {false};
uint8_t code_position = 0;
int current_dial_position = 0;
bool current_direction = true;
int steps_since_last_number = 0;
bool lock_started = false;
int target_number = 0;
bool target_direction = true;
unsigned long last_flash_time = 0;
bool flashing = false;
int flash_count = 0;
bool waiting_for_correct_number = true;
unsigned long last_correct_time = 0;
#define COOLDOWN_MS 1000

// ===== IDLE TIMEOUT RESET =====
unsigned long last_interaction_time = 0;
#define IDLE_TIMEOUT_MS 20000

// ===== GLOBAL OBJECTS =====
static knob_handle_t s_knob = 0;
int encoder_position = 0;
int last_encoder_position = 0;
bool haptic_available = false;

// ===== SCREEN DIMENSIONS =====
#define SCREEN_WIDTH 360
#define SCREEN_HEIGHT 360
#define CENTER_X 180
#define CENTER_Y 180

// ===== DIAL CONFIGURATION =====
#define DIAL_RADIUS          130
#define MARKER_RADIUS        100
#define NUMBER_RADIUS        155
#define MAJOR_DOT_SIZE       12
#define MEDIUM_DOT_SIZE      12
#define MINOR_DOT_SIZE       8
#define MARKER_DOT_SIZE      18

// Dot colors
#define MAJOR_DOT_COLOR      lv_color_make(0, 0, 250)  // Bright yellow
#define MEDIUM_DOT_COLOR     lv_color_make(0, 200, 120)  // Brass
#define MINOR_DOT_COLOR      lv_color_make(0, 200, 250)  // Light gray
#define MARKER_DOT_COLOR     lv_color_make(255, 0, 0)    // Red

// Number label settings
#define SHOW_ALL_NUMBERS     true
#define NUMBER_FONT          &lv_font_montserrat_14
#define NUMBER_COLOR         lv_color_make(220, 220, 200)
#define NUMBER_OFFSET_X    -5       // Adjust for font width
#define NUMBER_OFFSET_Y    -7        // Adjust for font height

// Center number settings
#define CENTER_FONT          &lv_font_montserrat_48
#define CENTER_COLOR         lv_color_make(255, 255, 255)

// Status text settings
#define STATUS_FONT          &lv_font_montserrat_16
#define STATUS_COLOR         lv_color_make(255, 255, 200)

// ===== UI ELEMENTS =====
lv_obj_t *dial_marker;
lv_obj_t *number_label;
lv_obj_t *status_label;

// ===== DIAL POSITIONS =====
const char* dial_numbers[] = {
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
  "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
  "30", "31", "32", "33", "34", "35", "36", "37", "38", "39"
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===== FUNCTION DECLARATIONS =====
void create_dial_ui();
void update_dial_display();
void update_dial_marker_position();
void check_encoder();
void check_lock_progress();
void check_idle_timeout();
void reset_lock();
bool init_haptic();
void play_haptic(uint8_t effect);
void sendESPNOWMessage(const char* cmd, bool unlocked, uint8_t step);

// ===== ESPNOW FUNCTION =====
void sendESPNOWMessage(const char* cmd, bool unlocked, uint8_t step) {
    strcpy(espnowData.command, cmd);
    espnowData.unlocked = unlocked;
    espnowData.step = step;
    espnowData.total_steps = CODE_LENGTH;
    
    if (strcmp(cmd, "UNLOCK") == 0) {
        memcpy(espnowData.code, entered_code, sizeof(entered_code));
    } else {
        memcpy(espnowData.code, secret_code, sizeof(secret_code));
    }
    
    if (!broadcast_peer.send_message((uint8_t *)&espnowData, sizeof(espnowData))) {
        Serial.print("Failed to send: ");
        Serial.println(cmd);
    } else {
        Serial.print("Sent: ");
        Serial.println(cmd);
    }
}

// ===== HAPTIC FUNCTIONS =====
bool init_haptic() {
  Serial.println("Initializing I2C for haptic motor...");

  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = HAPTIC_SDA,
    .scl_io_num = HAPTIC_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {
      .clk_speed = I2C_MASTER_FREQ_HZ,
    },
    .clk_flags = 0,
  };

  esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
  if (err != ESP_OK) {
    Serial.print("i2c_param_config failed: ");
    Serial.println(err);
    return false;
  }

  err = i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                           I2C_MASTER_RX_BUF_DISABLE,
                           I2C_MASTER_TX_BUF_DISABLE, 0);
  if (err != ESP_OK) {
    Serial.print("i2c_driver_install failed: ");
    Serial.println(err);
    return false;
  }

  delay(100);

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  if (err != ESP_OK) {
    Serial.print("Haptic motor not found at 0x5A. Error: ");
    Serial.println(err);
    i2c_driver_delete(I2C_MASTER_NUM);
    return false;
  }

  Serial.println("Haptic motor found. Initializing DRV2605...");

  uint8_t data[2];

  data[0] = 0x01;
  data[1] = 0x00;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  delay(10);

  data[0] = 0x01;
  data[1] = 0x00;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  data[0] = 0x03;
  data[1] = 0x01;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  Serial.println("Haptic motor ready");
  return true;
}

void play_haptic(uint8_t effect) {
  if (!haptic_available) return;

  uint8_t data[2];
  i2c_cmd_handle_t cmd;

  data[0] = 0x04;
  data[1] = effect;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  data[0] = 0x05;
  data[1] = 0x00;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  data[0] = 0x0C;
  data[1] = 0x01;
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (0x5A << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, data, 2, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
}

// ===== ENCODER CALLBACKS =====
static void _knob_left_cb(void *arg, void *data) {
  encoder_position--;
  current_direction = false;

  static unsigned long last_haptic = 0;
  if (millis() - last_haptic > 30 && haptic_available) {
    play_haptic(HAPTIC_TICK);
    last_haptic = millis();
  }
}

static void _knob_right_cb(void *arg, void *data) {
  encoder_position++;
  current_direction = true;

  static unsigned long last_haptic = 0;
  if (millis() - last_haptic > 30 && haptic_available) {
    play_haptic(HAPTIC_TICK);
    last_haptic = millis();
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("========================================");
  Serial.println("ROTARY COMBO LOCK - ESP32-S3");
  Serial.println("========================================");

  // Initialize WiFi and ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // Initialize broadcast peer
  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer");
    delay(5000);
  } else {
    Serial.println("ESP-NOW Broadcast peer initialized");
  }

  haptic_available = init_haptic();

  lcd_lvgl_Init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

  knob_config_t cfg = {
    .gpio_encoder_a = EXAMPLE_ENCODER_ECA_PIN,
    .gpio_encoder_b = EXAMPLE_ENCODER_ECB_PIN,
  };

  s_knob = iot_knob_create(&cfg);
  if (NULL == s_knob) {
    Serial.println("Encoder initialization failed");
  } else {
    Serial.println("Encoder ready");
  }

  iot_knob_register_cb(s_knob, KNOB_LEFT, _knob_left_cb, NULL);
  iot_knob_register_cb(s_knob, KNOB_RIGHT, _knob_right_cb, NULL);
  iot_knob_resume();

  create_dial_ui();

  target_number = secret_code[0];
  target_direction = secret_directions[0];

  last_interaction_time = millis();

  Serial.println("Combination: 25R-10L-35R-5L");
  Serial.println("Turn RIGHT 20+ steps to begin");

  if (haptic_available) {
    delay(500);
    play_haptic(HAPTIC_CLICK);
    delay(200);
    play_haptic(HAPTIC_CLICK);
  }
}

// ===== LOOP =====
void loop() {
  check_encoder();
  check_lock_progress();
  check_idle_timeout();

  if (flashing) {
    if (millis() - last_flash_time > 150) {
      if (flash_count < 3) {
        flash_count++;
        last_flash_time = millis();
      } else {
        flashing = false;
      }
    }
  }

  delay(10);
}

// ===== ENCODER HANDLER =====
void check_encoder() {
  if (encoder_position != last_encoder_position) {
    int change = encoder_position - last_encoder_position;

    last_interaction_time = millis();

    if (change > 0) {
      current_dial_position++;
      if (current_dial_position > 39) current_dial_position = 0;

      steps_since_last_number++;

      if (!lock_started && steps_since_last_number >= 20) {
        lock_started = true;
        Serial.println("\nLock sequence started");

        if (haptic_available) {
          play_haptic(HAPTIC_START);
        }
      }
    } else if (change < 0) {
      current_dial_position--;
      if (current_dial_position < 0) current_dial_position = 39;

      if (!lock_started) {
        steps_since_last_number = 0;
        Serial.println("LEFT -> Reset start counter");

        if (haptic_available) {
          play_haptic(HAPTIC_WRONG);
        }
      } else {
        steps_since_last_number++;
      }
    }

    last_encoder_position = encoder_position;
    update_dial_display();
    update_dial_marker_position();
  }
}

// ===== LOCK LOGIC =====
void check_lock_progress() {
  if (!lock_started) return;

  if (waiting_for_correct_number) {
    if (millis() - last_correct_time < COOLDOWN_MS) return;

    if (current_dial_position == target_number && current_direction == target_direction) {
      last_correct_time = millis();
      last_interaction_time = millis();

      entered_code[code_position] = target_number;
      entered_directions[code_position] = target_direction;

      Serial.print("\nCorrect! Number ");
      Serial.print(target_number);
      Serial.print(target_direction ? "R" : "L");
      Serial.println(" entered");

      // Send progress update
      sendESPNOWMessage("PROGRESS", false, code_position + 1);

      if (haptic_available) {
        play_haptic(HAPTIC_CORRECT);
      }

      flashing = true;
      flash_count = 0;
      last_flash_time = millis();

      code_position++;

      if (code_position < CODE_LENGTH) {
        target_number = secret_code[code_position];
        target_direction = secret_directions[code_position];
        steps_since_last_number = 0;

        Serial.print("Next target: ");
        Serial.print(target_number);
        Serial.println(target_direction ? "R" : "L");
      } else {
        bool correct = true;
        for (int i = 0; i < CODE_LENGTH; i++) {
          if (entered_code[i] != secret_code[i] || entered_directions[i] != secret_directions[i]) {
            correct = false;
            break;
          }
        }

        if (correct) {
          Serial.println("\nSAFE UNLOCKED! Combination correct!");

          // Send unlock signal
          sendESPNOWMessage("UNLOCK", true, CODE_LENGTH);

          if (haptic_available) {
            play_haptic(HAPTIC_UNLOCK);
          }

          lv_label_set_text(status_label, "SAFE UNLOCKED!");
          lv_obj_set_style_text_color(status_label, lv_color_make(50, 255, 50), 0);

          waiting_for_correct_number = false;
          
          // Clear status text after 10 seconds
          static unsigned long unlock_time = 0;
          unlock_time = millis();
          while (millis() - unlock_time < 10000) {
            delay(10);
          }
          lv_label_set_text(status_label, "");
        } else {
          Serial.println("\nWrong combination. Try again.");

          // Send error signal
          sendESPNOWMessage("ERROR", false, 0);

          if (haptic_available) {
            play_haptic(HAPTIC_WRONG);
          }

          delay(2000);
          reset_lock();
        }
      }
    }
  }
}

// ===== RESET FUNCTION =====
void reset_lock() {
  Serial.println("\n=== LOCK RESET ===");

  // Send reset signal
  sendESPNOWMessage("RESET", false, 0);

  if (haptic_available) {
    play_haptic(HAPTIC_RESET);
  }

  lock_started = false;
  code_position = 0;
  current_dial_position = 0;
  steps_since_last_number = 0;
  target_number = secret_code[0];
  target_direction = secret_directions[0];
  waiting_for_correct_number = true;
  last_correct_time = 0;

  encoder_position = 0;
  last_encoder_position = 0;

  if (s_knob) {
    iot_knob_clear_count_value(s_knob);
  }

  for (int i = 0; i < CODE_LENGTH; i++) {
    entered_code[i] = 0;
    entered_directions[i] = false;
  }

  lv_label_set_text(status_label, "");
  lv_obj_set_style_text_color(status_label, STATUS_COLOR, 0);

  update_dial_marker_position();

  Serial.println("Lock reset! Turn RIGHT 20+ steps to start");
  update_dial_display();
}

// ===== UI FUNCTIONS =====
void create_dial_ui() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(15, 20, 25), 0);

  Serial.println("Creating dial UI...");

  for (int i = 0; i < 40; i++) {
    float angle_deg = i * 9.0;
    float angle_rad = (angle_deg - 90) * M_PI / 180.0;

    int dot_x = CENTER_X + DIAL_RADIUS * cos(angle_rad);
    int dot_y = CENTER_Y + DIAL_RADIUS * sin(angle_rad);
    int num_x = CENTER_X + NUMBER_RADIUS * cos(angle_rad);
    int num_y = CENTER_Y + NUMBER_RADIUS * sin(angle_rad);

    lv_obj_t *dot = lv_obj_create(lv_scr_act());

    if (i % 10 == 0) {
      lv_obj_set_size(dot, MAJOR_DOT_SIZE, MAJOR_DOT_SIZE);
      lv_obj_set_style_bg_color(dot, MAJOR_DOT_COLOR, 0);
      lv_obj_set_style_radius(dot, MAJOR_DOT_SIZE / 2, 0);
      lv_obj_set_pos(dot, dot_x - MAJOR_DOT_SIZE/2, dot_y - MAJOR_DOT_SIZE/2);
    } else if (i % 5 == 0) {
      lv_obj_set_size(dot, MEDIUM_DOT_SIZE, MEDIUM_DOT_SIZE);
      lv_obj_set_style_bg_color(dot, MEDIUM_DOT_COLOR, 0);
      lv_obj_set_style_radius(dot, MEDIUM_DOT_SIZE / 2, 0);
      lv_obj_set_pos(dot, dot_x - MEDIUM_DOT_SIZE/2, dot_y - MEDIUM_DOT_SIZE/2);
    } else {
      lv_obj_set_size(dot, MINOR_DOT_SIZE, MINOR_DOT_SIZE);
      lv_obj_set_style_bg_color(dot, MINOR_DOT_COLOR, 0);
      lv_obj_set_style_radius(dot, MINOR_DOT_SIZE / 2, 0);
      lv_obj_set_pos(dot, dot_x - MINOR_DOT_SIZE/2, dot_y - MINOR_DOT_SIZE/2);
    }

    lv_obj_set_style_border_width(dot, 0, 0);

    if (SHOW_ALL_NUMBERS) {
      lv_obj_t *num_label = lv_label_create(lv_scr_act());
      lv_label_set_text(num_label, dial_numbers[i]);
      lv_obj_set_style_text_color(num_label, NUMBER_COLOR, 0);
      lv_obj_set_style_text_font(num_label, NUMBER_FONT, 0);
      lv_obj_set_pos(num_label, num_x + NUMBER_OFFSET_X, num_y + NUMBER_OFFSET_Y);
    }
  }

  dial_marker = lv_obj_create(lv_scr_act());
  lv_obj_set_size(dial_marker, MARKER_DOT_SIZE, MARKER_DOT_SIZE);
  lv_obj_set_style_radius(dial_marker, MARKER_DOT_SIZE / 2, 0);
  lv_obj_set_style_bg_color(dial_marker, MARKER_DOT_COLOR, 0);
  lv_obj_set_style_border_width(dial_marker, 0, 0);
  update_dial_marker_position();

  number_label = lv_label_create(lv_scr_act());
  lv_label_set_text(number_label, "0");
  lv_obj_set_style_text_color(number_label, CENTER_COLOR, 0);
  lv_obj_set_style_text_font(number_label, CENTER_FONT, 0);
  lv_obj_align(number_label, LV_ALIGN_CENTER, 0, 0);

  status_label = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label, "");
  lv_obj_set_style_text_color(status_label, STATUS_COLOR, 0);
  lv_obj_set_style_text_font(status_label, STATUS_FONT, 0);
  lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -30);
}

void update_dial_display() {
  lv_label_set_text(number_label, dial_numbers[current_dial_position]);
  
  // Status text is always empty except during unlock
  lv_label_set_text(status_label, "");
}

void update_dial_marker_position() {
  float angle_deg = current_dial_position * 9.0;
  float angle_rad = (angle_deg - 90) * M_PI / 180.0;
  int marker_x = CENTER_X + MARKER_RADIUS * cos(angle_rad);
  int marker_y = CENTER_Y + MARKER_RADIUS * sin(angle_rad);
  lv_obj_set_pos(dial_marker, marker_x - MARKER_DOT_SIZE/2, marker_y - MARKER_DOT_SIZE/2);
}

void check_idle_timeout() {
  if (millis() - last_interaction_time > IDLE_TIMEOUT_MS) {
    Serial.println("\n=== IDLE TIMEOUT - AUTO RESET ===");
    reset_lock();
    last_interaction_time = millis();
  }
}
