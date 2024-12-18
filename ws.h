#ifndef WS_H
#define WS_H

#include <Arduino.h>
#include "types.h"  // 包含共享类型定义

// 声明外部变量
extern AsyncWebSocket ws;
extern AnalogChannel analogChannels[12];
extern RelayChannel relayChannels[4];

// 将电压值映射到物理量（使用多点校准）
float mapVoltageToPhysical(float voltage, const AnalogChannel& channel);

// 读取指定通道的模拟量值
float readAnalogValue(int channel);

// 修改继电器控制函数
void setRelayState(int channel, bool state);

// 函数实现...
float mapVoltageToPhysical(float voltage, const AnalogChannel& channel) {
    if (channel.numPoints < 2) return 0.0;  // 至少需要2个校准点
    
    // 限制电压值范围
    if (voltage <= channel.calibPoints[0].voltage) 
        return channel.calibPoints[0].physical;
    if (voltage >= channel.calibPoints[channel.numPoints-1].voltage) 
        return channel.calibPoints[channel.numPoints-1].physical;
    
    // 查找电压值所在的区间
    for (int i = 0; i < channel.numPoints - 1; i++) {
        if (voltage >= channel.calibPoints[i].voltage && 
            voltage <= channel.calibPoints[i+1].voltage) {
            // 线性插值
            float ratio = (voltage - channel.calibPoints[i].voltage) / 
                         (channel.calibPoints[i+1].voltage - channel.calibPoints[i].voltage);
            return channel.calibPoints[i].physical + 
                   ratio * (channel.calibPoints[i+1].physical - channel.calibPoints[i].physical);
        }
    }
    return 0.0;
}

float readAnalogValue(int channel) {
    if (channel < 0 || channel >= 12) return 0.0;
    
    int rawValue = analogRead(analogChannels[channel].gpio);
    // 限制最大电压为3.0V
    float voltage = min((rawValue * 3.3f) / 4095.0f, 3.0f);
    return mapVoltageToPhysical(voltage, analogChannels[channel]);
}

void setRelayState(int channel, bool state) {
    if (channel >= 0 && channel < 4) {
        // 添加调试输出
        Serial.printf("setRelayState: channel=%d, state=%d, gpio=%d\n", 
            channel, state, relayChannels[channel].gpio);
        
        relayChannels[channel].state = state;
        // 继电器高电平触发
        digitalWrite(relayChannels[channel].gpio, state ? HIGH : LOW);
        
        // 验证GPIO状态
        int pinState = digitalRead(relayChannels[channel].gpio);
        Serial.printf("GPIO%d state verification: %d (expected: %d)\n", 
            relayChannels[channel].gpio, 
            pinState,
            state ? HIGH : LOW);
    }
}

#endif