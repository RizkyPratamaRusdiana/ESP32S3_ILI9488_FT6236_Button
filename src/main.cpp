#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <Adafruit_FT6206.h>

// ========================================================
//  📘  WIRING GUIDE (ESP32-S3 DevKitC + TFT ILI9488 + FT6236)
// ========================================================
//  ⚙️  TFT ILI9488 (SPI Interface)
//  ┌────────────────────┬───────────────┬──────────────────────┐
//  │ TFT PIN            │ ESP32-S3 PIN │ Keterangan            │
//  ├────────────────────┼───────────────┼──────────────────────┤
//  │ VCC                │ 3.3V         │ Power 3.3 V (jangan 5 V)│
//  │ GND                │ GND          │ Ground                │
//  │ CS                 │ — (NC)       │ Tidak digunakan (1 SPI bus)│
//  │ RESET              │ GPIO10       │ Reset LCD             │
//  │ DC / RS            │ GPIO14       │ Data/Command select   │
//  │ SDI / MOSI         │ GPIO11       │ SPI MOSI              │
//  │ SDO / MISO         │ GPIO13       │ SPI MISO              │
//  │ SCK                │ GPIO12       │ SPI Clock             │
//  │ LED / BL           │ 3.3V         │ Backlight aktif HIGH  │
//  └────────────────────┴───────────────┴──────────────────────┘
//
//  ⚙️  TOUCH FT6236 (I2C Interface)
//  ┌────────────────────┬───────────────┬──────────────────────┐
//  │ TOUCH PIN          │ ESP32-S3 PIN │ Keterangan            │
//  ├────────────────────┼───────────────┼──────────────────────┤
//  │ VCC                │ 3.3V         │ Power 3.3 V          │
//  │ GND                │ GND          │ Ground               │
//  │ SDA                │ GPIO8        │ I2C Data             │
//  │ SCL                │ GPIO9        │ I2C Clock            │
//  │ INT (opsional)     │ — (NC)       │ Tidak wajib dipakai  │
//  └────────────────────┴───────────────┴──────────────────────┘
//
//  💡 Opsional tambahan:
//  - LED indikator → GPIO2 (bisa dipakai untuk tombol LED ON/OFF)
// ========================================================


// ========================================================
//  KONFIGURASI LOVYANGFX UNTUK ESP32-S3 + ILI9488 (SPI)
// ========================================================
class LGFX_ESP32S3_ILI9488 : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX_ESP32S3_ILI9488(void) {
    { // --- BUS SPI ---
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 12;  // SCK
      cfg.pin_mosi = 11;  // MOSI
      cfg.pin_miso = 13;  // MISO
      cfg.pin_dc   = 14;  // DC/RS
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // --- PANEL ---
      auto cfg = _panel_instance.config();
      cfg.pin_cs   = -1;   // Tidak digunakan
      cfg.pin_rst  = 10;   // Reset pin
      cfg.pin_busy = -1;   // Tidak ada
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert   = false;
      cfg.readable = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

// ========================================================
//  INSTANCE OBJEK
// ========================================================
LGFX_ESP32S3_ILI9488 tft;
Adafruit_FT6206 touch = Adafruit_FT6206();

const uint8_t ROTATION = 1;  // Landscape mode

// ========================================================
//  TOUCH CALIBRATION (hasil uji nyata, ubah jika offset)
// ========================================================
int TOUCH_MAP_X1 = 8;
int TOUCH_MAP_X2 = 296;
int SCREEN_MAP_X1 = 320;
int SCREEN_MAP_X2 = 0;

int TOUCH_MAP_Y1 = 0;
int TOUCH_MAP_Y2 = 479;
int SCREEN_MAP_Y1 = 0;
int SCREEN_MAP_Y2 = 479;

// ========================================================
//  STRUKTUR DAN DEFINISI BUTTON
// ========================================================
struct Button {
  int x, y, w, h;
  String label;
  uint16_t color;
  uint16_t textColor;
  bool enabled;

  bool contains(int px, int py) {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
  }
};

// Tombol menu utama
Button buttons[] = {
  {20,  55, 210, 55, "LED ON",     TFT_GREEN,  TFT_WHITE, true},
  {20, 120, 210, 55, "LED OFF",    TFT_RED,    TFT_WHITE, true},
  {20, 185, 210, 55, "SENSOR",     TFT_BLUE,   TFT_WHITE, true},
  {20, 250, 210, 55, "SETTINGS",   TFT_ORANGE, TFT_BLACK, true},
  {250, 55, 210, 55, "INFO",       TFT_CYAN,   TFT_BLACK, true},
  {250, 120, 210, 55, "STATUS",    TFT_PURPLE, TFT_WHITE, true},
  {250, 185, 210, 55, "WIFI",      TFT_YELLOW, TFT_BLACK, true},
  {250, 250, 210, 55, "CLEAR",     TFT_DARKGREY, TFT_WHITE, true}
};

const int NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);
String statusText = "Ready";
uint16_t statusColor = TFT_WHITE;

// ========================================================
//  FUNGSI: Konversi koordinat sentuhan ke layar TFT
// ========================================================
void getTouchCoordinates(int &screenX, int &screenY) {
  TS_Point p = touch.getPoint();
  screenY = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, SCREEN_MAP_X1, SCREEN_MAP_X2);
  screenX = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, SCREEN_MAP_Y1, SCREEN_MAP_Y2);
  screenX = constrain(screenX, 0, tft.width() - 1);
  screenY = constrain(screenY, 0, tft.height() - 1);
}

// ========================================================
//  FUNGSI: Gambar tombol ke layar
// ========================================================
void drawButton(Button &btn, bool pressed = false) {
  uint16_t bgColor = pressed ? TFT_WHITE : btn.color;
  uint16_t txtColor = pressed ? btn.color : btn.textColor;

  if (!btn.enabled) {
    bgColor = TFT_DARKGREY;
    txtColor = TFT_LIGHTGREY;
  }

  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, bgColor);
  tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 8, TFT_WHITE);

  tft.setFont(&fonts::Font2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(txtColor);
  tft.drawString(btn.label, btn.x + btn.w / 2, btn.y + btn.h / 2);
}

// ========================================================
//  FUNGSI: Update status bar atas
// ========================================================
void updateStatus(const String &text, uint16_t color = TFT_WHITE) {
  statusText = text;
  statusColor = color;

  // Header bar
  tft.fillRect(0, 0, 480, 40, TFT_NAVY);
  tft.setFont(&fonts::Font2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("MENU UTAMA", 10, 20);

  // Status kanan
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(color);
  tft.drawString(text, 470, 20);

  Serial.println("Status: " + text);
}

// ========================================================
//  FUNGSI: Handle interaksi tombol
// ========================================================
void handleButtonClick(int index) {
  drawButton(buttons[index], true);
  delay(150);
  drawButton(buttons[index], false);

  switch (index) {
    case 0: updateStatus("LED: ON", TFT_GREEN); break;
    case 1: updateStatus("LED: OFF", TFT_RED); break;
    case 2:
      updateStatus("Reading sensor...", TFT_CYAN);
      delay(500);
      updateStatus("Temp: 25C", TFT_YELLOW);
      break;
    case 3: updateStatus("Settings opened", TFT_ORANGE); break;
    case 4: updateStatus("ESP32-S3 v1.0", TFT_CYAN); break;
    case 5: updateStatus("System OK", TFT_GREEN); break;
    case 6: updateStatus("WiFi: Disconnected", TFT_YELLOW); break;
    case 7: updateStatus("Ready", TFT_WHITE); break;
  }
}

// ========================================================
//  SETUP
// ========================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 MENU SYSTEM ===");

  // Inisialisasi LCD
  tft.init();
  tft.setRotation(ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft.setFont(&fonts::Font2);

  // Inisialisasi Touch
  Wire.begin(8, 9);
  Wire.setClock(400000);

  bool touchOK = false;
  uint8_t thresholds[] = {40, 128, 20, 60};
  for (int i = 0; i < 4; i++) {
    if (touch.begin(thresholds[i])) {
      touchOK = true;
      Serial.printf("Touch OK (threshold: %d)\n", thresholds[i]);
      break;
    }
    delay(100);
  }

  if (!touchOK) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(20, 50);
    tft.println("TOUCH INIT FAILED!");
    while (1) delay(1000);
  }

  updateStatus("Ready", TFT_WHITE);
  for (int i = 0; i < NUM_BUTTONS; i++) drawButton(buttons[i]);
  Serial.println("Menu ready!");
}

// ========================================================
//  LOOP
// ========================================================
unsigned long lastTouchTime = 0;
bool wasTouched = false;

void loop() {
  if (touch.touched()) {
    unsigned long now = millis();
    if (!wasTouched && (now - lastTouchTime > 200)) {
      int screenX, screenY;
      getTouchCoordinates(screenX, screenY);
      Serial.printf("Touch at: (%d, %d)\n", screenX, screenY);

      for (int i = 0; i < NUM_BUTTONS; i++) {
        if (buttons[i].enabled && buttons[i].contains(screenX, screenY)) {
          Serial.printf("Button %d (%s) pressed!\n",
                        i, buttons[i].label.c_str());
          handleButtonClick(i);
          break;
        }
      }
      lastTouchTime = now;
      wasTouched = true;
    }
  } else {
    wasTouched = false;
  }
  delay(10);
}
