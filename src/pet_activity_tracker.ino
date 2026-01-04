
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>
#include <LittleFS.h>
#include <esp_sleep.h>


// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust if needed


// Simulated IMU structure
struct IMUdata {
  float x;
  float y;
  float z;
};


// Simulated QMI8658 sensor
class SimulatedQMI8658 {
  bool motionDetected = false;
  void (*wakeupCallback)() = nullptr;


public:
  bool begin() { return true; }


  void setWakeupMotionEventCallBack(void (*callback)()) {
    wakeupCallback = callback;
  }


  void simulateMotion() {
    if (random(100) < 10 && !motionDetected) {
      motionDetected = true;
      if (wakeupCallback) wakeupCallback();
    } else {
      motionDetected = false;
    }
  }


  bool readFromFifo(IMUdata* acc, int accCount, IMUdata* gyr, int gyrCount) {
    for (int i = 0; i < accCount; i++) {
      acc[i].x = random(-2000, 2000) / 1000.0;
      acc[i].y = random(-2000, 2000) / 1000.0;
      acc[i].z = random(-2000, 2000) / 1000.0;
    }
    for (int i = 0; i < gyrCount; i++) {
      gyr[i].x = random(-500, 500) / 100.0;
      gyr[i].y = random(-500, 500) / 100.0;
      gyr[i].z = random(-500, 500) / 100.0;
    }
    return true;
  }


  void configWakeOnMotion() {}
};


// Time manager
class TimeManager {
public:
  void begin() {
    configTime(0, 0, "pool.ntp.org");
  }


  String formatTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
  }
};


// Activity classification
enum ActivityType {
  Resting,
  Walking,
  Running,
  Playing
};


class ActivityClassifier {
public:
  ActivityType classify(const IMUdata& acc) {
    float mag = sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
    if (mag < 0.2) return Resting;
    else if (mag < 0.7) return Walking;
    else if (mag < 1.5) return Running;
    else return Playing;
  }
};


// Globals
SimulatedQMI8658 imu;
TimeManager timeManager;
ActivityClassifier classifier;
volatile bool motionDetected = false;


// Wakeup ISR
void wakeupCallback() {
  motionDetected = true;
}


void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // ESP32-WROOM-32 default I2C pins


  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dog Tracker");


  // Initialize LittleFS and format if necessary
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed, formatting...");
    if (!LittleFS.begin(true)) {
      Serial.println("LittleFS format failed!");
      return;
    }
  } else {
    Serial.println("LittleFS Mounted Successfully!");
  }


  imu.begin();
  imu.setWakeupMotionEventCallBack(wakeupCallback);
  timeManager.begin();
}


void loop() {
  imu.simulateMotion();


  if (!motionDetected) {
    Serial.println("No motion, sleeping for 2 min...");
    esp_sleep_enable_timer_wakeup(2 * 60 * 1000000);
    esp_deep_sleep_start();
  }


  IMUdata acc[1], gyr[1];
  imu.readFromFifo(acc, 1, gyr, 1);


  ActivityType activity = classifier.classify(acc[0]);


  String activityName;
  switch (activity) {
    case Resting: activityName = "Resting"; break;
    case Walking: activityName = "Walking"; break;
    case Running: activityName = "Running"; break;
    case Playing: activityName = "Playing"; break;
  }


  // Display on LCD
  lcd.setCursor(0, 1);
  lcd.print(activityName + "     ");


  // Log to LittleFS
  File file = LittleFS.open("/activity.log", FILE_APPEND);
  if (file) {
    file.printf("%s -> %s\n", timeManager.formatTime().c_str(), activityName.c_str());
    file.close();
  }


  Serial.printf("Logged: %s\n", activityName.c_str());


  motionDetected = false;
  delay(5000);
}
