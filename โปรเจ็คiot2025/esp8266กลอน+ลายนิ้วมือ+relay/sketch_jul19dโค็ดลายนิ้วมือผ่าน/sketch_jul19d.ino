#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

// WiFi
const char* ssid = "4G-UFI-599F";
const char* password = "1234567890";

// Telegram
const String TELEGRAM_TOKEN = "7450797409:AAFEdHPmihPcHpgV0CEbKOs00luVk8Wt2ug";
const String CHAT_ID = "-4855310547";

// Battery config
const float VREF = 1.0;
const float MAX_BATT = 4.2;
const float MIN_BATT = 3.3;
const float FACTOR = 4.9;
const int BATTERY_CHECK_INTERVAL = 3600000; // 1 hr
unsigned long lastBatteryCheck = 0;

// Relay
const int RELAY_PIN = 5; // D1

// Fingerprint sensor
SoftwareSerial mySerial(D7, D8); // RX=D4(GPIO2), TX=D3(GPIO0)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600);

  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor ready");
  } else {
    Serial.println("❌ Fingerprint sensor not found");
    while (true) delay(1000);
  }
}

void loop() {
  checkBattery();

  int result = finger.getImage();
  if (result != FINGERPRINT_OK) return;

  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) return;

  result = finger.fingerSearch();
  if (result == FINGERPRINT_OK) {
    Serial.printf("🔓 Access Granted [ID %d]\n", finger.fingerID);
    unlockRelay();
    sendTelegram("✅ ปลดล็อคสำเร็จ\n🧠 ลายนิ้วมือ ID: " + String(finger.fingerID));
  } else {
    Serial.println("🚨 Unknown fingerprint!");
    sendTelegram("🚨 พบลายนิ้วมือไม่รู้จัก!\n❗ มีความพยายามเข้าระบบ");
  }

  delay(2000); // หน่วงก่อน loop ใหม่
}

void unlockRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(5000); // ปลดล็อค 5 วินาที
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("🔐 Lock restored");
}

void checkBattery() {
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    float raw = analogRead(A0);
    float vout = raw * (VREF / 1023.0);
    float vbatt = vout * FACTOR;
    vbatt = constrain(vbatt, 3.0, 4.3);
    int percent = map(vbatt * 100, MIN_BATT * 100, MAX_BATT * 100, 0, 100);
    percent = constrain(percent, 0, 100);
    Serial.printf("🔋 Battery: %.2f V (%d%%)\n", vbatt, percent);

    if (percent <= 20) {
      sendTelegram("⚠️ แบตเตอรี่อ่อน\n🔋 " + String(vbatt, 2) + "V (" + String(percent) + "%)");
    }

    lastBatteryCheck = millis();
  }
}

void sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + TELEGRAM_TOKEN + "/sendMessage";
  https.begin(client, url);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String data = "chat_id=" + CHAT_ID + "&text=" + message;
  int httpCode = https.POST(data);
  Serial.printf("📨 Telegram Status: %d\n", httpCode);
  https.end();
}
