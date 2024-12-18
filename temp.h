#ifndef TEMP_H
#define TEMP_H

#include <Adafruit_MAX31865.h>

// 定义温度传感器类型
enum TempSensorType {
    PT100 = 0,
    PT1000 = 1
};

// 温度传感器配置结构
struct TempSensorConfig {
    bool enabled;
    String name;
    TempSensorType type;
    float lastTemp;
    uint8_t cs_pin;
};

// 定义三个温度传感器
extern Adafruit_MAX31865 thermo1;
extern Adafruit_MAX31865 thermo2;

// 传感器配置数组
extern TempSensorConfig tempSensors[2];

// 初始化温度传感器
void initTempSensors() {
    // 设置SPI引脚
    const int sck_pin = 12;   // GPIO12 - SCK
    const int sdo_pin = 13;   // GPIO13 - SDO (MISO)
    const int sdi_pin = 11;   // GPIO11 - SDI (MOSI)
    
    // 设置引脚模式
    pinMode(sck_pin, OUTPUT);
    pinMode(sdo_pin, INPUT);
    pinMode(sdi_pin, OUTPUT);
    
    // 设置CS引脚
    pinMode(10, OUTPUT);   // CS1 - GPIO10
    pinMode(39, OUTPUT);   // CS2 - GPIO39
    
    // 初始化每个传感器为2线制模式
    thermo1.begin(MAX31865_2WIRE);  // 2线制PT100/PT1000
    thermo2.begin(MAX31865_2WIRE);  // 2线制PT100/PT1000
    
    // 初始CS引脚为高电平
    digitalWrite(10, HIGH);
    digitalWrite(39, HIGH);
}

// 读取温度数据
void readTemperatures() {
    for(int i = 0; i < 2; i++) {
        if(tempSensors[i].enabled) {
            Adafruit_MAX31865* sensor;
            switch(i) {
                case 0: sensor = &thermo1; break;
                case 1: sensor = &thermo2; break;
            }
            
            // 使用标准参数
            float r0 = 100.0;        // 0℃时的标准电阻值
            float rref = 439.78;     // 根据实测值校准的参考电阻
            float alpha = 0.00385;   // 温度系数
            
            // 读取温度前检查故障
            uint8_t fault = sensor->readFault();
            if(fault) {
                Serial.printf("Sensor %d fault: %d\n", i, fault);
                sensor->clearFault();
                continue;
            }
            
            // 读取原始RTD值并计算实际电阻
            float rtd = sensor->readRTD();
            float ratio = rtd / 32768.0;
            float measured_r = ratio * rref;
            
            // 计算温度
            float temp = (measured_r / r0 - 1.0) / alpha;
            
            // 保存温度值
            tempSensors[i].lastTemp = temp;
            
            // 输出详细调试信息
            Serial.printf("\nSensor %d Detailed Info:\n", i);
            Serial.printf("RTD Raw Value: %.2f\n", rtd);
            Serial.printf("Ratio: %.6f\n", ratio);
            Serial.printf("Measured Resistance: %.2f ohms\n", measured_r);
            Serial.printf("Target R at 18°C: 107.02 ohms\n");  // 添加目标值显示
            Serial.printf("R0 (0°C standard): %.2f ohms\n", r0);
            Serial.printf("Reference R: %.2f ohms\n", rref);
            Serial.printf("Temperature Coefficient: %.6f\n", alpha);
            Serial.printf("Final Temperature: %.2f°C\n", temp);
            
            // 添加电阻误差显示
            float r_error = measured_r - 107.02;
            Serial.printf("Resistance Error: %.3f ohms\n", r_error);
        }
    }
}

// 保存温度传感器配置
void saveTempConfig() {
    Serial.println("Saving temperature config to file...");
    
    File file = SPIFFS.open("/temp_config.json", "w");
    if(!file) {
        Serial.println("Failed to open temp config file for writing");
        return;
    }

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.createNestedArray("sensors");

    for(int i = 0; i < 2; i++) {
        JsonObject sensor = array.createNestedObject();
        sensor["enabled"] = tempSensors[i].enabled;
        sensor["name"] = tempSensors[i].name;
        sensor["type"] = (int)tempSensors[i].type;
        sensor["cs_pin"] = tempSensors[i].cs_pin;
        
        Serial.printf("Saving sensor %d: enabled=%d, name=%s, type=%d, cs_pin=%d\n",
            i, tempSensors[i].enabled, tempSensors[i].name.c_str(), 
            (int)tempSensors[i].type, tempSensors[i].cs_pin);
    }

    bool success = serializeJson(doc, file);
    file.close();

    if(success) {
        Serial.println("Temperature config saved successfully");
    } else {
        Serial.println("Failed to write temperature config");
    }
}

// 加载温度传感器配��
void loadTempConfig() {
    Serial.println("Loading temperature config...");
    
    if(!SPIFFS.exists("/temp_config.json")) {
        Serial.println("No temperature config file found, using defaults");
        return;
    }

    File file = SPIFFS.open("/temp_config.json", "r");
    if(!file) {
        Serial.println("Failed to open temperature config file");
        return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if(error) {
        Serial.println("Failed to parse temperature config file");
        return;
    }
    
    JsonArray array = doc["sensors"].as<JsonArray>();
    int i = 0;
    for(JsonVariant v : array) {
        if(i < 2) {
            tempSensors[i].enabled = v["enabled"].as<bool>();
            tempSensors[i].name = v["name"].as<String>();
            tempSensors[i].type = (TempSensorType)v["type"].as<int>();
            if(v.containsKey("cs_pin")) {
                tempSensors[i].cs_pin = v["cs_pin"].as<uint8_t>();
            }
            
            Serial.printf("Loaded sensor %d: enabled=%d, name=%s, type=%d, cs_pin=%d\n",
                i, tempSensors[i].enabled, tempSensors[i].name.c_str(), 
                (int)tempSensors[i].type, tempSensors[i].cs_pin);
            
            i++;
        }
    }
    Serial.println("Temperature config loaded successfully");
}

#endif