
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

#define ENROLL_BUTTON     D1
#define DELETE_BUTTON     D2
#define DELETE_ALL_BUTTON D3

SoftwareSerial mySerial(D7, D8); // RX, TX
Adafruit_Fingerprint finger(&mySerial);

uint8_t id = 1;

void setup() {
  Serial.begin(115200);
  delay(2000); // รอให้เซ็นเซอร์พร้อมก่อน

  pinMode(ENROLL_BUTTON, INPUT_PULLUP);
  pinMode(DELETE_BUTTON, INPUT_PULLUP);
  pinMode(DELETE_ALL_BUTTON, INPUT_PULLUP);

  Serial.println("🔐 เริ่มต้นระบบลายนิ้วมือ...");

  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("✅ พบเซ็นเซอร์ลายนิ้วมือแล้ว");
  } else {
    Serial.println("❌ ไม่พบเซ็นเซอร์ กรุณาตรวจสาย TX/RX และไฟเลี้ยง");
    while (1) delay(1);
  }
}

void loop() {
  if (digitalRead(ENROLL_BUTTON) == LOW) {
    while (digitalRead(ENROLL_BUTTON) == LOW); // รอปล่อยปุ่ม
    enrollFingerprint(id++);
  }

  if (digitalRead(DELETE_BUTTON) == LOW) {
    while (digitalRead(DELETE_BUTTON) == LOW);
    Serial.println("🔢 กรุณาใส่ ID ที่ต้องการลบใน Serial Monitor:");
    while (!Serial.available());
    int deleteId = Serial.parseInt();
    while (Serial.available()) Serial.read(); // ล้าง Serial buffer
    deleteFingerprint(deleteId);
  }

  if (digitalRead(DELETE_ALL_BUTTON) == LOW) {
    while (digitalRead(DELETE_ALL_BUTTON) == LOW);
    deleteAllFingerprints();
  }
}

void enrollFingerprint(int id) {
  Serial.print("📲 เพิ่มลายนิ้วมือ ID "); Serial.println(id);
  int p = -1;
  Serial.println("👉 วางนิ้วบนเซ็นเซอร์...");

  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p != FINGERPRINT_NOFINGER) {
      Serial.println("❌ อ่านภาพไม่สำเร็จ"); return;
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ แปลงภาพครั้งแรกไม่สำเร็จ"); return;
  }

  Serial.println("✋ ยกนิ้วออก...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);

  Serial.println("👉 วางนิ้วเดิมอีกครั้ง...");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p != FINGERPRINT_NOFINGER) {
      Serial.println("❌ อ่านภาพครั้งที่สองไม่สำเร็จ"); return;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ แปลงภาพครั้งที่สองไม่สำเร็จ"); return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ สร้างโมเดลลายนิ้วมือไม่สำเร็จ"); return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ บันทึกลายนิ้วมือสำเร็จ!");
  } else {
    Serial.println("❌ บันทึกลายนิ้วมือไม่สำเร็จ");
  }
}

void deleteFingerprint(int id) {
  int p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.print("✅ ลบลายนิ้วมือ ID "); Serial.print(id); Serial.println(" สำเร็จ");
  } else {
    Serial.println("❌ ลบลายนิ้วมือไม่สำเร็จ หรือไม่พบ ID นี้");
  }
}

void deleteAllFingerprints() {
  int p = finger.emptyDatabase();
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ ลบลายนิ้วมือทั้งหมดสำเร็จ");
  } else {
    Serial.println("❌ ลบทั้งหมดไม่สำเร็จ");
  }
}
