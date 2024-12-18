#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "types.h"
#include "html.h"
#include "ws.h"
#include "temp.h"

// Constants for WiFi connection
const char* AP_SSID = "YourAPSSID";  // Set your AP's SSID
const char* AP_PASS = "12345678";  // Set your AP's password
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;  // 30 seconds
const unsigned long WIFI_RETRY_DELAY = 500;  // 500 milliseconds

// Global variables
AsyncWebServer server(80);
String sta_ssid;
String sta_pass;
bool wifi_connected = false;
String systemTitle = "ESP32 控制系统";  // 默认标题

// 定义 WebSocket 对象
AsyncWebSocket ws("/ws");

// 定义模拟量通道数组
AnalogChannel analogChannels[12];

// 定义继电器通道数组
RelayChannel relayChannels[4];

unsigned long lastSensorUpdate = 0;
unsigned long lastTempUpdate = 0;  // 添加温度更新时间戳
const unsigned long SENSOR_UPDATE_INTERVAL = 200;  // ADC采样间隔200ms (1秒5次)
const unsigned long TEMP_UPDATE_INTERVAL = 1000;   // 温度采样间隔1000ms (1秒1次)
const unsigned long DATA_SEND_INTERVAL = 1000;     // 数据发送间隔1秒
unsigned long lastDataSendTime = 0;                // 上次发送数据的时间

// 定义温度传感器对象
Adafruit_MAX31865 thermo1(10);   // CS pin: GPIO10
Adafruit_MAX31865 thermo2(39);   // CS pin: GPIO39

// 定义温度传感器配置数组
TempSensorConfig tempSensors[2] = {
    {false, "温度传感器 1", PT100, 0.0, 10},  // 默认关闭，CS: GPIO10
    {false, "温度传感器 2", PT100, 0.0, 39}   // 默认关闭，CS: GPIO39
};

bool connectWiFi(const char* ssid, const char* password);
void loadConfig();
void saveConfig();
void initAnalogChannels();
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void sendSensorData();
void saveAnalogConfig(int channelIndex);
void loadAnalogConfig();
void initRelayChannels();
void saveRelayConfig();
void loadRelayConfig();

// Function to initialize WiFi and server settings
void setupWiFiAndServer() {
    WiFi.mode(WIFI_AP_STA);

    // 设置 AP 的 IP 地址、网关和子网掩码
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("AP started successfully: %s\n", WiFi.softAPIP().toString().c_str());

    // Setup server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateHomePage());
    });

    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        String ip = wifi_connected ? WiFi.localIP().toString() : "";
        request->send(200, "text/html", generateWiFiPage(wifi_connected, sta_ssid, ip));
    });

    server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid") && request->hasParam("pass")) {
            sta_ssid = request->getParam("ssid")->value();
            sta_pass = request->getParam("pass")->value();
            if (connectWiFi(sta_ssid.c_str(), sta_pass.c_str())) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(200, "text/plain", "Failed");
            }
        } else {
            request->send(400, "text/plain", "Missing Parameters");
        }
    });

    server.on("/analog", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateAnalogConfigPage());
    });

    server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateRelayConfigPage());
    });

    server.on("/save_analog_config", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(400, "text/plain", "Invalid Request");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, (char*)data);
        
        if (error) {
            Serial.println("Failed to parse JSON");
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }

        JsonArray config = doc["config"].as<JsonArray>();
        if(config.size() == 1) {
            // 单通道配置
            JsonObject channelConfig = config[0];
            int channelIndex = channelConfig["channel"].as<int>();
            
            if(channelIndex >= 0 && channelIndex < 12) {
                analogChannels[channelIndex].enabled = channelConfig["enabled"].as<bool>();
                analogChannels[channelIndex].name = channelConfig["name"].as<String>();
                analogChannels[channelIndex].unit = channelConfig["unit"].as<String>();
                
                // 根据通道号计算正确的 GPIO
                if(channelIndex < 8) {
                    analogChannels[channelIndex].gpio = channelIndex + 1;  // GPIO1-8
                } else {
                    analogChannels[channelIndex].gpio = channelIndex + 7;  // GPIO15-18
                }
                
                // 处理其他配置项
                analogChannels[channelIndex].filterLimit = channelConfig["filterLimit"].as<int>();
                analogChannels[channelIndex].compensation = channelConfig["compensation"].as<float>();
                
                // 处理校准点
                JsonArray points = channelConfig["calibPoints"].as<JsonArray>();
                analogChannels[channelIndex].numPoints = 0;
                for(JsonVariant p : points) {
                    if(analogChannels[channelIndex].numPoints < 8) {
                        float voltage = p["voltage"].as<float>();
                        float physical = p["physical"].as<float>();
                        if (!isnan(voltage) && !isnan(physical)) {
                            analogChannels[channelIndex].calibPoints[analogChannels[channelIndex].numPoints].voltage = voltage;
                            analogChannels[channelIndex].calibPoints[analogChannels[channelIndex].numPoints].physical = physical;
                            analogChannels[channelIndex].numPoints++;
                        }
                    }
                }
                
                // 如果没���有效的校准点，设置默认值
                if(analogChannels[channelIndex].numPoints == 0) {
                    analogChannels[channelIndex].calibPoints[0] = {0.0, 0.0};
                    analogChannels[channelIndex].calibPoints[1] = {3.3, 100.0};
                    analogChannels[channelIndex].numPoints = 2;
                }
                
                // 保存配置到文件
                saveAnalogConfig(channelIndex);
                
                Serial.printf("Channel %d config saved successfully\n", channelIndex);
                request->send(200, "text/plain", "OK");  // 确保返回 200 状态码和 "OK" 响应
                return;
            }
        }
        
        Serial.println("Invalid channel or config");
        request->send(200, "text/plain", "OK");  // 即使出错也返回 200
    });

    server.on("/get_analog_config", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(8192);
        JsonArray channels = doc.createNestedArray("channels");
        
        for(int i = 0; i < 12; i++) {  // 从8改为12
            JsonObject channel = channels.createNestedObject();
            channel["enabled"] = analogChannels[i].enabled;
            channel["name"] = analogChannels[i].name;
            channel["unit"] = analogChannels[i].unit;
            channel["gpio"] = analogChannels[i].gpio;
            channel["filterLimit"] = analogChannels[i].filterLimit;
            channel["compensation"] = analogChannels[i].compensation;
            
            JsonArray points = channel.createNestedArray("calibPoints");
            for(int j = 0; j < analogChannels[i].numPoints; j++) {
                JsonObject point = points.createNestedObject();
                point["voltage"] = analogChannels[i].calibPoints[j].voltage;
                point["physical"] = analogChannels[i].calibPoints[j].physical;
            }
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/get_relay_status", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.createNestedArray("relays");
        
        for(int i = 0; i < 4; i++) {
            JsonObject relay = array.createNestedObject();
            relay["name"] = relayChannels[i].name;
            relay["gpio"] = relayChannels[i].gpio;
            relay["state"] = relayChannels[i].state;
            relay["mode"] = relayChannels[i].mode;
            relay["autoRunning"] = relayChannels[i].autoRunning;
            relay["currentCycles"] = relayChannels[i].currentCycles;
            relay["maxCycles"] = relayChannels[i].maxCycles;
            relay["onTime"] = relayChannels[i].onTime;
            relay["offTime"] = relayChannels[i].offTime;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/relay/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("channel", true) && request->hasParam("state", true)) {
            int channel = request->getParam("channel", true)->value().toInt();
            bool state = request->getParam("state", true)->value() == "1";
            
            if (channel >= 0 && channel < 4 && relayChannels[channel].mode == MANUAL) {
                // 添加调试输出
                Serial.printf("Setting relay %d to %s (GPIO%d)\n", 
                    channel, 
                    state ? "ON" : "OFF",
                    relayChannels[channel].gpio);
                
                // 设置继电器状态
                relayChannels[channel].state = state;
                // 继电器高电平触发
                digitalWrite(relayChannels[channel].gpio, state ? HIGH : LOW);
                
                // 再证GPIO状态
                int pinState = digitalRead(relayChannels[channel].gpio);
                Serial.printf("GPIO%d state after setting: %d (expected: %d)\n", 
                    relayChannels[channel].gpio, 
                    pinState,
                    state ? HIGH : LOW);
                
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid Request");
            }
        } else {
            request->send(400, "text/plain", "Missing Parameters");
        }
    });

    server.on("/get_relay_config", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.createNestedArray("relays");
        
        for(int i = 0; i < 4; i++) {
            JsonObject relay = array.createNestedObject();
            relay["name"] = relayChannels[i].name;
            relay["gpio"] = relayChannels[i].gpio;
            relay["state"] = relayChannels[i].state;
            relay["mode"] = relayChannels[i].mode;
            relay["autoRunning"] = relayChannels[i].autoRunning;
            relay["currentCycles"] = relayChannels[i].currentCycles;
            relay["maxCycles"] = relayChannels[i].maxCycles;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/save_relay_config", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(400, "text/plain", "Invalid Request");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, (char*)data);
        
        JsonArray config = doc["config"].as<JsonArray>();
        for(JsonVariant v : config) {
            int channel = v["channel"].as<int>();
            if(channel >= 0 && channel < 4) {
                relayChannels[channel].name = v["name"].as<String>();
                relayChannels[channel].mode = (RelayMode)v["mode"].as<int>();
                if(relayChannels[channel].mode == AUTOMATIC) {
                    relayChannels[channel].onTime = v["onTime"].as<unsigned long>();
                    relayChannels[channel].offTime = v["offTime"].as<unsigned long>();
                    relayChannels[channel].maxCycles = v["maxCycles"].as<unsigned int>();
                    relayChannels[channel].currentCycles = 0;
                    relayChannels[channel].autoRunning = false;
                }
            }
        }
        
        saveRelayConfig();
        request->send(200, "text/plain", "Configuration saved");
    });

    // 添加自动运行控制路由
    server.on("/relay/auto_control", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("channel", true) && request->hasParam("running", true)) {
            int channel = request->getParam("channel", true)->value().toInt();
            bool running = request->getParam("running", true)->value() == "1";
            
            if (channel >= 0 && channel < 4 && relayChannels[channel].mode == AUTOMATIC) {
                relayChannels[channel].autoRunning = running;
                if(running) {
                    // 只在开始新的自动运行时才重置循环次数
                    if(relayChannels[channel].currentCycles >= relayChannels[channel].maxCycles) {
                        relayChannels[channel].currentCycles = 0;
                    }
                    relayChannels[channel].lastToggleTime = millis();
                    // 从关闭状态开始
                    setRelayState(channel, false);
                } else {
                    // 停止动运行时，保持当前状态
                    relayChannels[channel].autoRunning = false;
                }
                saveRelayConfig();  // 保存状态
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid Request");
            }
        } else {
            request->send(400, "text/plain", "Missing Parameters");
        }
    });

    server.on("/save_title", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("title", true)) {
            String newTitle = request->getParam("title", true)->value();
            newTitle.trim();
            
            if(newTitle.length() > 0) {
                systemTitle = newTitle;
                
                // 保存标题到SPIFFS
                File file = SPIFFS.open("/title.txt", "w");
                if(file) {
                    file.print(systemTitle);
                    file.close();
                    Serial.println("System title saved: " + systemTitle);
                    request->send(200, "text/plain", "OK");
                } else {
                    Serial.println("Failed to open title file for writing");
                    request->send(500, "text/plain", "Failed to save");
                }
            } else {
                request->send(400, "text/plain", "Title cannot be empty");
            }
        } else {
            request->send(400, "text/plain", "Missing title");
        }
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // 添加限幅值更路由（放在 server.begin() 之前）
    server.on("/save_filter_limit", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("channel", true) && request->hasParam("limit", true)) {
            int channel = request->getParam("channel", true)->value().toInt();
            int limit = request->getParam("limit", true)->value().toInt();
            
            if (channel >= 0 && channel < 8 && limit >= 0) {
                analogChannels[channel].filterLimit = limit;
                saveAnalogConfig(channel);  // 保存到配置文件
                Serial.printf("Updated channel %d filter limit to %d\n", channel, limit);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid parameters");
            }
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    // 添加温度配置页面路由
    server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateTempConfigPage());
    });

    // 添加获取温度配置路由
    server.on("/get_temp_config", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.createNestedArray("sensors");
        
        for(int i = 0; i < 2; i++) {
            JsonObject sensor = array.createNestedObject();
            sensor["enabled"] = tempSensors[i].enabled;
            sensor["name"] = tempSensors[i].name;
            sensor["type"] = (int)tempSensors[i].type;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 添加保存温度配置路由
    server.on("/save_temp_config", HTTP_POST, [](AsyncWebServerRequest *request) {
        // 空处理函数，用于处理不带数据的POST请求
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // 这里是处理带数据的POST请求的函数
        Serial.println("Received temp config save request");
        
        if(len == 0) {
            Serial.println("Empty request body");
            request->send(400, "text/plain", "Empty request");
            return;
        }
        
        // 创建一个临时缓冲区来存储请求数据
        char* json = new char[len + 1];
        memcpy(json, data, len);
        json[len] = '\0';
        
        Serial.printf("Received JSON: %s\n", json);
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, json);
        
        delete[] json;
        
        if(error) {
            Serial.printf("Failed to parse JSON: %s\n", error.c_str());
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        int sensorIndex = doc["index"].as<int>();
        if(sensorIndex >= 0 && sensorIndex < 2) {
            JsonObject config = doc["config"];
            
            Serial.printf("Saving config for sensor %d\n", sensorIndex);
            Serial.printf("Enabled: %d\n", config["enabled"].as<bool>());
            Serial.printf("Name: %s\n", config["name"].as<const char*>());
            Serial.printf("Type: %d\n", config["type"].as<int>());
            
            // 更新配置
            tempSensors[sensorIndex].enabled = config["enabled"].as<bool>();
            tempSensors[sensorIndex].name = config["name"].as<String>();
            tempSensors[sensorIndex].type = (TempSensorType)config["type"].as<int>();
            
            // 重新初始化传感器
            switch(sensorIndex) {
                case 0:
                    thermo1.begin(MAX31865_2WIRE);
                    break;
                case 1:
                    thermo2.begin(MAX31865_2WIRE);
                    break;
            }
            
            // 保存配置到文件
            saveTempConfig();
            
            Serial.println("Temperature config saved successfully");
            request->send(200, "text/plain", "OK");
        } else {
            Serial.println("Invalid sensor index");
            request->send(400, "text/plain", "Invalid sensor index");
        }
    });

    // 保持在后
    server.begin();
    Serial.println("Web server started successfully");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // 始化 SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to initialize SPIFFS!");
        return;
    }
    Serial.println("SPIFFS initialized successfully");

    // 删除旧的继电器配置文件
    if(SPIFFS.exists("/relay_config.json")) {
        SPIFFS.remove("/relay_config.json");
        Serial.println("Removed old relay config file");
    }

    // 加载系统标题
    if(SPIFFS.exists("/title.txt")) {
        File file = SPIFFS.open("/title.txt", "r");
        if(file) {
            systemTitle = file.readString();
            systemTitle.trim();
            file.close();
        }
    }

    // 先加载 WiFi 配置
    loadConfig();
    
    // 设置 WiFi 模式并启动 AP
    WiFi.mode(WIFI_AP_STA);
    
    // 设置 AP 的 IP 地址、网关和子网掩码
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS);
    
    // 如果有保存的 WiFi 配置，尝试连接
    if (!sta_ssid.isEmpty() && !sta_pass.isEmpty()) {
        Serial.printf("Attempting to connect to saved WiFi: %s\n", sta_ssid.c_str());
        if(connectWiFi(sta_ssid.c_str(), sta_pass.c_str())) {
            Serial.println("Connected to saved WiFi successfully");
        } else {
            Serial.println("Failed to connect to saved WiFi");
        }
    }

    // 加载其他配置
    loadAnalogConfig();
    loadRelayConfig();
    loadTempConfig();
    
    // 初始化设备
    initAnalogChannels();
    initRelayChannels();
    initTempSensors();
    
    setupWiFiAndServer();
}

void loop() {
    if (wifi_connected && WiFi.status() != WL_CONNECTED) {
        wifi_connected = false;
        Serial.println("WiFi connection lost, trying to reconnect...");
        connectWiFi(sta_ssid.c_str(), sta_pass.c_str());
    }

    unsigned long currentMillis = millis();

    // 处理ADC采样
    if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
        lastSensorUpdate = currentMillis;
        sampleADC();
    }

    // 处理温度采样（每秒一次）
    if (currentMillis - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {
        lastTempUpdate = currentMillis;
        readTemperatures();
    }

    // 处理数据发送
    if (currentMillis - lastDataSendTime >= DATA_SEND_INTERVAL) {
        lastDataSendTime = currentMillis;
        sendSensorData();
    }

    // 处理继电器自动控制
    for(int i = 0; i < 4; i++) {
        if(relayChannels[i].mode == AUTOMATIC && relayChannels[i].autoRunning) {
            unsigned long currentTime = millis();
            unsigned long timeInState = currentTime - relayChannels[i].lastToggleTime;
            
            // 根据当前状态决定使用哪个时间间
            unsigned long waitTime = relayChannels[i].state ? 
                                   relayChannels[i].onTime : 
                                   relayChannels[i].offTime;
            
            if(timeInState >= waitTime) {
                // 检查是否达到最大循环次数
                if(relayChannels[i].currentCycles >= relayChannels[i].maxCycles) {
                    relayChannels[i].autoRunning = false;
                    saveRelayConfig();  // 保存当前状态
                    continue;
                }
                
                // 切换状态
                bool newState = !relayChannels[i].state;
                setRelayState(i, newState);
                relayChannels[i].lastToggleTime = currentTime;
                
                // 如果从开到关，增加循环计数
                if(!newState) {  // 当切换到关闭状态时
                    relayChannels[i].currentCycles++;
                    saveRelayConfig();  // 保存循环次数
                }
            }
        }
    }
}

bool connectWiFi(const char* ssid, const char* password) {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    
    WiFi.begin(ssid, password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        sta_ssid = ssid;      // 保存SSID
        sta_pass = password;   // 保存密码
        saveConfig();         // 保存到文件
        Serial.printf("Connected successfully. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        wifi_connected = false;
        Serial.println("Failed to connect");
        return false;
    }
}

void loadConfig() {
    Serial.println("Loading WiFi config...");
    
    if(!SPIFFS.exists("/wifi_config.json")) {
        Serial.println("No WiFi config file found");
        return;
    }
    
    File file = SPIFFS.open("/wifi_config.json", "r");
    if(!file) {
        Serial.println("Failed to open WiFi config file");
        return;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if(error) {
        Serial.println("Failed to parse WiFi config file");
        return;
    }
    
    sta_ssid = doc["ssid"].as<String>();
    sta_pass = doc["password"].as<String>();
    
    Serial.println("WiFi config loaded successfully");
    Serial.printf("Loaded SSID: %s\n", sta_ssid.c_str());
}

void saveConfig() {
    Serial.println("Saving WiFi config...");
    
    DynamicJsonDocument doc(256);
    doc["ssid"] = sta_ssid;
    doc["password"] = sta_pass;
    
    File file = SPIFFS.open("/wifi_config.json", "w");
    if(!file) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    
    if(serializeJson(doc, file)) {
        Serial.println("WiFi config saved successfully");
        Serial.printf("Saved SSID: %s\n", sta_ssid.c_str());
    } else {
        Serial.println("Failed to write config file");
    }
    file.close();
}

// 修改 initAnalogChannels 函数中的GPIO映射
void initAnalogChannels() {
    for (int i = 0; i < 12; i++) {  // 从8改为12
        if (analogChannels[i].name.isEmpty()) {
            analogChannels[i].enabled = false;
            analogChannels[i].name = "传感器 " + String(i + 1);
            analogChannels[i].unit = "单位";
            
            // 修改GPIO映射
            if (i < 8) {
                analogChannels[i].gpio = i + 1;  // GPIO1-8
            } else {
                analogChannels[i].gpio = i + 7;  // GPIO15-18
            }
            
            // 其他初始化代码保持不变
            analogChannels[i].numPoints = 2;
            analogChannels[i].calibPoints[0] = {0.0, 0.0};
            analogChannels[i].calibPoints[1] = {3.3, 100.0};
            analogChannels[i].filterLimit = 20;  // 设置默认限幅值
            analogChannels[i].compensation = 0;  // 设置默认补偿值
            analogChannels[i].sampleCount = 0;
            analogChannels[i].lastSecondValue = 0;
            analogChannels[i].currentValue = 0;
            analogChannels[i].difference = 0;
        }
    }
}

// 修改 initRelayChannels 函数中的GPIO映射
void initRelayChannels() {
    for (int i = 0; i < 4; i++) {
        // 如果继电器没有配置，才设置默认值
        if (relayChannels[i].name.isEmpty()) {
            relayChannels[i].name = "电器 " + String(i + 1);
            
            // 修改GPIO映射
            switch(i) {
                case 0: relayChannels[i].gpio = 21; break;  // 继电器1 - GPIO21
                case 1: relayChannels[i].gpio = 45; break;  // 继电器2 - GPIO45
                case 2: relayChannels[i].gpio = 47; break;  // 继电器3 - GPIO47
                case 3: relayChannels[i].gpio = 48; break;  // 继电器4 - GPIO48
            }
            
            relayChannels[i].state = false;
            relayChannels[i].mode = MANUAL;
            relayChannels[i].autoRunning = false;
            relayChannels[i].currentCycles = 0;
            relayChannels[i].onTime = 1000;  // 默认1秒
            relayChannels[i].offTime = 1000; // 默认1秒
            relayChannels[i].maxCycles = 1;  // 默认1次
        }
        
        // 确保GPIO设置正确
        pinMode(relayChannels[i].gpio, OUTPUT);
        digitalWrite(relayChannels[i].gpio, LOW);  // 初始状态为低电平（关闭）
        
        // 添加调试输出
        Serial.printf("Initialized relay %d: GPIO%d, initial state: LOW (OFF)\n", 
            i, relayChannels[i].gpio);
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // Optionally send initial data or a welcome message to the client
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Handle incoming data
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len) {
            // The whole message is in a single frame and we got all of it's data
            Serial.printf("WebSocket message: %s\n", data);
        }
    }
}

void sendSensorData() {
    DynamicJsonDocument doc(2048);
    JsonArray values = doc.createNestedArray("values");
    
    for(int i = 0; i < 12; i++) {
        if(analogChannels[i].enabled) {
            int rawValue = analogChannels[i].currentValue;
            // 计算未校准的电压
            float uncalibrated_voltage = (rawValue * 3.3f) / 4095.0f;
            // 应用校准
            float calibrated_voltage = calibrateVoltage(uncalibrated_voltage);
            // 使用校准后的电压计算物理值
            float physicalValue = mapVoltageToPhysical(calibrated_voltage, analogChannels[i]);
            physicalValue += analogChannels[i].compensation;

            JsonObject channel = values.createNestedObject();
            channel["channel"] = i;
            channel["name"] = analogChannels[i].name;
            channel["gpio"] = analogChannels[i].gpio;
            channel["rawValue"] = rawValue;
            channel["rawVoltage"] = uncalibrated_voltage;  // 添加未校准电压
            channel["voltage"] = calibrated_voltage;  // 校准后的电压
            channel["value"] = physicalValue;
            channel["unit"] = analogChannels[i].unit;
            channel["filterLimit"] = analogChannels[i].filterLimit;
            channel["compensation"] = analogChannels[i].compensation;
            channel["difference"] = analogChannels[i].difference;
        }
    }
    
    // 添加温度数据
    JsonArray temps = doc.createNestedArray("temperatures");
    for(int i = 0; i < 2; i++) {
        if(tempSensors[i].enabled) {
            JsonObject temp = temps.createNestedObject();
            temp["name"] = tempSensors[i].name;
            temp["value"] = tempSensors[i].lastTemp;
            temp["type"] = (int)tempSensors[i].type;
            temp["enabled"] = tempSensors[i].enabled;
            
            // 计算电阻值
            Adafruit_MAX31865* sensor;
            switch(i) {
                case 0: sensor = &thermo1; break;
                case 1: sensor = &thermo2; break;
            }
            float rtd = sensor->readRTD();
            float ratio = rtd / 32768.0;
            float resistance = ratio * 439.78;  // 使用新的参考电阻值
            temp["resistance"] = resistance;
            temp["fault"] = sensor->readFault();
        }
    }
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// 修改 saveAnalogConfig 数，添补偿值的保存
void saveAnalogConfig(int channelIndex) {
    Serial.println("Saving analog config...");
    
    File file = SPIFFS.open("/analog_config.json", "w");
    if(!file) {
        Serial.println("Failed to open analog config file for writing");
        return;
    }

    DynamicJsonDocument doc(8192);
    JsonArray channels = doc.createNestedArray("channels");

    // 保存所有通道的配置
    for(int i = 0; i < 12; i++) {  // 从8改为12
        JsonObject channel = channels.createNestedObject();
        channel["enabled"] = analogChannels[i].enabled;
        channel["name"] = analogChannels[i].name;
        channel["unit"] = analogChannels[i].unit;
        channel["gpio"] = analogChannels[i].gpio;
        channel["numPoints"] = analogChannels[i].numPoints;
        
        JsonArray points = channel.createNestedArray("calibPoints");
        for(int j = 0; j < analogChannels[i].numPoints; j++) {
            JsonObject point = points.createNestedObject();
            point["voltage"] = analogChannels[i].calibPoints[j].voltage;
            point["physical"] = analogChannels[i].calibPoints[j].physical;
        }
        
        channel["filterLimit"] = analogChannels[i].filterLimit;
        channel["compensation"] = analogChannels[i].compensation;
    }

    if(serializeJson(doc, file)) {
        Serial.println("Analog config saved successfully");
    } else {
        Serial.println("Failed to write analog config");
    }
    file.close();
}

// 改 loadAnalogConfig 
void loadAnalogConfig() {
    if(SPIFFS.exists("/analog_config.json")) {
        File file = SPIFFS.open("/analog_config.json", "r");
        if(file) {
            Serial.println("Loading analog config...");
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, file);
            
            if (error) {
                Serial.println("Failed to parse analog config file");
                file.close();
                return;
            }
            
            JsonArray channels = doc["channels"].as<JsonArray>();
            int i = 0;
            for(JsonVariant v : channels) {
                if(i < 12) {  // 从8改为12
                    analogChannels[i].enabled = v["enabled"].as<bool>();
                    analogChannels[i].name = v["name"].as<String>();
                    analogChannels[i].unit = v["unit"].as<String>();
                    analogChannels[i].gpio = v["gpio"].as<int>();
                    analogChannels[i].numPoints = v["numPoints"].as<int>();
                    // 用 as<int>() 并置默认值
                    analogChannels[i].filterLimit = v["filterLimit"].as<int>();
                    if(analogChannels[i].filterLimit <= 0) {
                        analogChannels[i].filterLimit = 20;  // 设置认值
                    }
                    analogChannels[i].compensation = v["compensation"].as<float>();
                    
                    // 加载校准点
                    JsonArray points = v["calibPoints"].as<JsonArray>();
                    int j = 0;
                    for(JsonVariant p : points) {
                        if(j < 8) {
                            analogChannels[i].calibPoints[j].voltage = p["voltage"].as<float>();
                            analogChannels[i].calibPoints[j].physical = p["physical"].as<float>();
                            j++;
                        }
                    }
                    
                    // 如果没有有效的校准点，设置默认值
                    if(analogChannels[i].numPoints < 2) {
                        analogChannels[i].calibPoints[0] = {0.0, 0.0};
                        analogChannels[i].calibPoints[1] = {3.3, 100.0};
                        analogChannels[i].numPoints = 2;
                    }
                    
                    Serial.printf("Loaded channel %d: %s (filterLimit: %d)\n", 
                        i, 
                        analogChannels[i].name.c_str(),
                        analogChannels[i].filterLimit);
                    i++;
                }
            }
            file.close();
            Serial.println("Analog config loaded successfully");
        } else {
            Serial.println("Failed to open analog config file");
        }
    } else {
        Serial.println("No analog config file found, using defaults");
        for(int i = 0; i < 8; i++) {
            analogChannels[i].filterLimit = 20;  // 认值为20
        }
    }
}

// 修改 saveRelayConfig 函数添加错误处理和日志
void saveRelayConfig() {
    Serial.println("Saving relay config...");
    
    File file = SPIFFS.open("/relay_config.json", "w");
    if(!file) {
        Serial.println("Failed to open relay config file for writing");
        return;
    }

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.createNestedArray("relays");

    for(int i = 0; i < 4; i++) {
        JsonObject relay = array.createNestedObject();
        relay["name"] = relayChannels[i].name;
        relay["gpio"] = relayChannels[i].gpio;
        relay["mode"] = relayChannels[i].mode;
        relay["onTime"] = relayChannels[i].onTime;
        relay["offTime"] = relayChannels[i].offTime;
        relay["maxCycles"] = relayChannels[i].maxCycles;
        relay["currentCycles"] = relayChannels[i].currentCycles;
        relay["state"] = relayChannels[i].state;
        relay["autoRunning"] = relayChannels[i].autoRunning;
    }

    if(serializeJson(doc, file)) {
        Serial.println("Relay config saved successfully");
    } else {
        Serial.println("Failed to write relay config");
    }
    file.close();
}

// 修改 loadRelayConfig 函数，添加错误处理和日志
void loadRelayConfig() {
    if(SPIFFS.exists("/relay_config.json")) {
        File file = SPIFFS.open("/relay_config.json", "r");
        if(file) {
            Serial.println("Loading relay config...");
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (error) {
                Serial.println("Failed to parse relay config file");
                initDefaultRelayConfig();  // 使用默认配置
                return;
            }
            
            JsonArray array = doc["relays"].as<JsonArray>();
            int i = 0;
            for(JsonVariant v : array) {
                if(i < 4) {
                    relayChannels[i].name = v["name"].as<String>();
                    // 强制使用正确的 GPIO 映射
                    switch(i) {
                        case 0: relayChannels[i].gpio = 21; break;  // 继电器1 - GPIO21
                        case 1: relayChannels[i].gpio = 45; break;  // 继电器2 - GPIO45
                        case 2: relayChannels[i].gpio = 47; break;  // 继电器3 - GPIO47
                        case 3: relayChannels[i].gpio = 48; break;  // 继电器4 - GPIO48
                    }
                    relayChannels[i].mode = (RelayMode)v["mode"].as<int>();
                    relayChannels[i].onTime = v["onTime"].as<unsigned long>();
                    relayChannels[i].offTime = v["offTime"].as<unsigned long>();
                    relayChannels[i].maxCycles = v["maxCycles"].as<unsigned int>();
                    relayChannels[i].currentCycles = v["currentCycles"].as<unsigned int>();
                    relayChannels[i].state = false;  // 制初始状态为关闭
                    relayChannels[i].autoRunning = false;  // 强制自动运行为关闭
                    
                    // 初始化 GPIO
                    pinMode(relayChannels[i].gpio, OUTPUT);
                    digitalWrite(relayChannels[i].gpio, LOW);
                    
                    Serial.printf("Loaded relay %d: GPIO%d\n", i, relayChannels[i].gpio);
                    i++;
                }
            }
        } else {
            Serial.println("Failed to open relay config file");
            initDefaultRelayConfig();  // 使用默认配置
        }
    } else {
        Serial.println("No relay config file found, using defaults");
        initDefaultRelayConfig();  // 使用默认配置
    }
}

// 添加默认配置初始化函数
void initDefaultRelayConfig() {
    for(int i = 0; i < 4; i++) {
        relayChannels[i].name = "电器 " + String(i + 1);
        switch(i) {
            case 0: relayChannels[i].gpio = 21; break;  // 继电器1 - GPIO21
            case 1: relayChannels[i].gpio = 45; break;  // 继电器2 - GPIO45
            case 2: relayChannels[i].gpio = 47; break;  // 继电器3 - GPIO47
            case 3: relayChannels[i].gpio = 48; break;  // 继电器4 - GPIO48
        }
        relayChannels[i].state = false;
        relayChannels[i].mode = MANUAL;
        relayChannels[i].autoRunning = false;
        relayChannels[i].currentCycles = 0;
        relayChannels[i].onTime = 1000;
        relayChannels[i].offTime = 1000;
        relayChannels[i].maxCycles = 1;
        
        pinMode(relayChannels[i].gpio, OUTPUT);
        digitalWrite(relayChannels[i].gpio, LOW);
        
        Serial.printf("Initialized relay %d: GPIO%d, initial state: LOW (OFF)\n", 
            i, relayChannels[i].gpio);
    }
    
    // 保存默认配置
    saveRelayConfig();
}

// 修改 sampleADC 函数添差值储
void sampleADC() {
    for(int i = 0; i < 12; i++) {  // 从8改为12
        if(analogChannels[i].enabled) {
            int rawValue = analogRead(analogChannels[i].gpio);
            
            // 存入采缓冲
            analogChannels[i].sampleBuffer[analogChannels[i].sampleCount] = rawValue;
            analogChannels[i].sampleCount++;
            
            // 当收集到5个样本时，取��间值
            if(analogChannels[i].sampleCount >= 5) {
                // 对本进行排序
                int tempBuffer[5];
                memcpy(tempBuffer, analogChannels[i].sampleBuffer, sizeof(tempBuffer));
                for(int j = 0; j < 4; j++) {
                    for(int k = 0; k < 4-j; k++) {
                        if(tempBuffer[k] > tempBuffer[k+1]) {
                            int temp = tempBuffer[k];
                            tempBuffer[k] = tempBuffer[k+1];
                            tempBuffer[k+1] = temp;
                        }
                    }
                }
                
                // 获取新的中值
                int newMedian = tempBuffer[2];
                
                // 计算与上一秒值差值
                int diff = abs(newMedian - analogChannels[i].lastSecondValue);
                
                // 应用限幅滤波（与上一秒的比较）
                if (diff <= analogChannels[i].filterLimit) {
                    // 差值在限幅范围，继续使用上一秒值
                    analogChannels[i].currentValue = analogChannels[i].lastSecondValue;
                } else {
                    // 值超出限幅范围，使用新值
                    analogChannels[i].currentValue = newMedian;
                }

                // 存储实际差值用于示
                analogChannels[i].difference = diff;
                
                // 对于 GPIO1 打印详细信息
                if(i == 0) {
                    Serial.println("\nGPIO1 5个采样值:");
                    for(int j = 0; j < 5; j++) {
                        Serial.printf("%d ", analogChannels[i].sampleBuffer[j]);
                    }
                    Serial.printf("\n中值: %d", newMedian);
                    Serial.printf("\n上一秒值: %d", analogChannels[i].lastSecondValue);
                    Serial.printf("\n差值: %d", diff);
                    Serial.printf("\n限值: %d", analogChannels[i].filterLimit);
                    Serial.printf("\n最终使用值: %d\n", analogChannels[i].currentValue);
                }
                
                // 更上一秒的值（移到这里）
                analogChannels[i].lastSecondValue = analogChannels[i].currentValue;
                analogChannels[i].sampleCount = 0;
            }
        }
    }
}

// 修改校准表定义
const ADCCalibPoint AnalogChannel::calibTable[31] = {
    {0.0f, 0.0f, 0.02f},
    {0.1f, 0.065f, 0.02f},
    {0.2f, 0.165f, 0.02f},
    {0.3f, 0.265f, 0.02f},
    {0.4f, 0.365f, 0.02f},
    {0.5f, 0.465f, 0.02f},
    {0.6f, 0.565f, 0.02f},
    {0.7f, 0.665f, 0.02f},
    {0.8f, 0.765f, 0.02f},
    {0.9f, 0.865f, 0.02f},
    {1.0f, 0.932f, 0.02f},
    {1.1f, 1.032f, 0.02f},
    {1.2f, 1.132f, 0.02f},
    {1.3f, 1.232f, 0.02f},
    {1.4f, 1.332f, 0.02f},
    {1.5f, 1.432f, 0.02f},
    {1.6f, 1.532f, 0.02f},
    {1.7f, 1.632f, 0.02f},
    {1.8f, 1.732f, 0.02f},
    {1.9f, 1.832f, 0.02f},
    {2.0f, 1.932f, 0.02f},
    {2.1f, 2.032f, 0.02f},
    {2.2f, 2.132f, 0.02f},
    {2.3f, 2.232f, 0.02f},
    {2.4f, 2.332f, 0.02f},
    {2.5f, 2.432f, 0.02f},
    {2.6f, 2.532f, 0.02f},
    {2.7f, 2.665f, 0.02f},
    {2.8f, 2.800f, 0.02f},
    {2.9f, 2.932f, 0.02f},
    {3.0f, 3.000f, 0.02f}  // 将最大值限制在3.0V
};

// 修改校准函数
float calibrateVoltage(float measured_voltage) {
    // 如果测量值超出范围，进行限制
    if (measured_voltage <= AnalogChannel::calibTable[0].measured_voltage) {
        return AnalogChannel::calibTable[0].input_voltage;
    }
    if (measured_voltage >= AnalogChannel::calibTable[30].measured_voltage) {
        return AnalogChannel::calibTable[30].input_voltage;
    }

    // 遍历所有校准点，查找匹配的范围
    for (int i = 0; i < 31; i++) {
        float target = AnalogChannel::calibTable[i].measured_voltage;
        float tolerance = AnalogChannel::calibTable[i].tolerance;
        
        // 如果测量值在当前校准点的容差范围内
        if (measured_voltage >= (target - tolerance) && 
            measured_voltage <= (target + tolerance)) {
            // 直接返回对应的校准值
            return AnalogChannel::calibTable[i].input_voltage;
        }
        
        // 如果测量值在两个校准点之间
        if (i < 30 && measured_voltage > target && 
            measured_voltage < AnalogChannel::calibTable[i + 1].measured_voltage) {
            
            float next_target = AnalogChannel::calibTable[i + 1].measured_voltage;
            float current_input = AnalogChannel::calibTable[i].input_voltage;
            float next_input = AnalogChannel::calibTable[i + 1].input_voltage;
            
            // 计算测量值在两个校准点之间的相对位置（0-1）
            float position = (measured_voltage - target) / (next_target - target);
            
            // 使用非线性映射，让结果更偏向于较近的校准点
            position = position < 0.5 ? 
                      2.0 * position * position :  // 更偏向前一个点
                      1.0 - 2.0 * (1.0 - position) * (1.0 - position);  // 更偏向后一个点
            
            // 计算校准后的值
            return current_input + (next_input - current_input) * position;
        }
    }
    
    return measured_voltage; // 如果出现意外情况
}

