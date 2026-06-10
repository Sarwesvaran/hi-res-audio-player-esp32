#include <Arduino.h>
#include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- NVS ERASE UTILITY ---");
  
  // Hard erase of the NVS partition
  esp_err_t err = nvs_flash_erase();
  if (err == ESP_OK) {
    Serial.println("NVS Partition Erased Successfully!");
  } else {
    Serial.printf("Error Erasing NVS: %s\n", esp_err_to_name(err));
  }
  
  // Re-initialize (optional but good practice)
  nvs_flash_init();
  Serial.println("NVS Re-initialized. You can now flash the main code.");
}

void loop() {}