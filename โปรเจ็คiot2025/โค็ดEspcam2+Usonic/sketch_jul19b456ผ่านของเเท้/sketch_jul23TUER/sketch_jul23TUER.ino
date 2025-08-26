#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <time.h>

// ===== WiFi Config =====
const char* ssid = "4G-UFI-221";
const char* password = "1234567890";

// ===== Telegram Config =====
String BOT_TOKEN = "7450797409:AAFEdHPmihPcHpgV0CEbKOs00luVk8Wt2ug";
String CHAT_ID = "-4855310547";

// ===== HC-SR04 Pins =====
#define TRIG_PIN 12
#define ECHO_PIN 13
#define DIST_THRESHOLD_CM 40
#define CAPTURE_INTERVAL 300000 // 5 นาที

// ===== Global =====
unsigned long lastCaptureTime = 0;
bool hasCaptured = false;

// ===== Function Prototypes =====
void initCamera();
long measureDistance();
void captureAndSendPhoto();
bool sendToTelegram(camera_fb_t *fb);
String getTimeString();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // ปิด brownout detector
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  configTime(7 * 3600, 0, "pool.ntp.org");  // เวลาไทย

  initCamera();
  Serial.println("📷 กล้องพร้อมใช้งานแล้ว รอวัตถุเข้ามาใกล้...");
}

void loop() {
  long distance = measureDistance();
  Serial.printf("📏 ระยะ: %ld cm\n", distance);
  unsigned long now = millis();

  if (distance > 0 && distance < DIST_THRESHOLD_CM) {
    if (!hasCaptured || now - lastCaptureTime >= CAPTURE_INTERVAL) {
      Serial.println("📸 ตรวจพบวัตถุใกล้ - กำลังถ่ายภาพ...");
      captureAndSendPhoto();
      lastCaptureTime = now;
      hasCaptured = true;
    }
  } else {
    hasCaptured = false;
  }

  delay(1000);
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

void captureAndSendPhoto() {
  for (int i = 0; i < 2; i++) {
    camera_fb_t *flush = esp_camera_fb_get();
    if (flush) esp_camera_fb_return(flush);
    delay(100);
  }

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
  client.setInsecure();

  Serial.println("🌐 เชื่อมต่อ Telegram...");
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("❌ ไม่สามารถเชื่อมต่อ Telegram");
    return false;
  }

  delay(100);  // ให้ TLS พร้อม

  String boundary = "----WebKitFormBoundary7MA4YWkTrZu0gW";
  String caption = "📦 วัตถุใหม่จาก CAM2\n🕒 เวลา: " + getTimeString();

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
