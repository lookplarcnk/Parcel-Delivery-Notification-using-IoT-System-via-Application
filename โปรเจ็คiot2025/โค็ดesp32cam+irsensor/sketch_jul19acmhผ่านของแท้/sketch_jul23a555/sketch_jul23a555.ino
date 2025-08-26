#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <time.h>

// ===== WiFi Configuration =====
const char* ssid = "4G-UFI-221";
const char* password = "1234567890";

// ===== Telegram Configuration =====
String BOT_TOKEN = "7450797409:AAFEdHPmihPcHpgV0CEbKOs00luVk8Wt2ug";
String CHAT_ID = "-4855310547";

// ===== IR Sensor Pin =====
#define IR_SENSOR_PIN 14

// ===== Global Variables =====
bool irLastState = HIGH;
bool triggerPending = false;
unsigned long triggerTime = 0;
const unsigned long waitTime = 3000; // รอ 3 วินาทีหลังพัสดุผ่าน

// ===== Function Declarations =====
void initCamera();
void checkIRSensor();
void captureAndSendPhoto();
bool sendToTelegram(camera_fb_t *fb);
String getTimeString();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // ปิด brownout detector
  Serial.begin(115200);
  delay(1000);

  pinMode(IR_SENSOR_PIN, INPUT);

  Serial.println("🔌 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
  } else {
    Serial.println("\n❌ WiFi connection failed");
    return;
  }

  configTime(7 * 3600, 0, "pool.ntp.org");  // เวลาไทย UTC+7

  initCamera();
  Serial.println("📷 กล้องพร้อมใช้งานแล้ว รอพัสดุผ่านเซ็นเซอร์...");
}

void loop() {
  checkIRSensor();
}

void checkIRSensor() {
  int irState = digitalRead(IR_SENSOR_PIN);

  // ตรวจจับการผ่านของพัสดุ (จาก LOW → HIGH)
  if (irLastState == LOW && irState == HIGH) {
    Serial.println("📦 พัสดุผ่านเซ็นเซอร์ → เริ่มนับถอยหลัง 3 วินาที");
    triggerPending = true;
    triggerTime = millis();
  }

  irLastState = irState;

  // ถ้าครบเวลาแล้ว → ถ่ายภาพ
  if (triggerPending && millis() - triggerTime >= waitTime) {
    Serial.println("📸 ครบ 3 วิหลังพัสดุผ่าน → ถ่ายภาพและส่ง Telegram");
    captureAndSendPhoto();
    triggerPending = false;
  }
}

void captureAndSendPhoto() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ ไม่สามารถถ่ายภาพได้");
    return;
  }

  Serial.printf("📷 ขนาดภาพ: %d bytes\n", fb->len);

  if (sendToTelegram(fb)) {
    Serial.println("✅ ส่งภาพผ่าน Telegram สำเร็จ");
  } else {
    Serial.println("⚠️ การส่ง Telegram ล้มเหลว");
  }

  esp_camera_fb_return(fb);
}

bool sendToTelegram(camera_fb_t *fb) {
  WiFiClientSecure client;
  client.setInsecure();  // ไม่ตรวจ cert

  Serial.println("🌐 เชื่อมต่อ Telegram...");
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("❌ ไม่สามารถเชื่อมต่อ Telegram");
    return false;
  }

  delay(100);  // รอ TLS ready

  String boundary = "----WebKitFormBoundary7MA4YWkTrZu0gW";
  String caption = "📦 พัสดุถูกเก็บไว้ในตู้แล้ว\n🕒 เวลา: " + getTimeString();
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID + "\r\n"
                "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption + "\r\n"
                "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";
  int contentLength = head.length() + fb->len + tail.length();

  client.println("POST /bot" + BOT_TOKEN + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLength));
  client.println("Connection: close");
  client.println();
  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(tail);

  String response = "";
  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 5000) {
    while (client.available()) {
      char c = client.read();
      response += c;
      timeout = millis();
    }
  }

  client.stop();
  Serial.println("📡 Telegram response:\n" + response);
  return response.indexOf("\"ok\":true") >= 0;
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "ไม่ทราบเวลา";
  char buf[64];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ กล้องไม่สามารถใช้งานได้");
    while (1) delay(100);
  } else {
    Serial.println("📷 กล้องพร้อมใช้งาน");
  }
}
