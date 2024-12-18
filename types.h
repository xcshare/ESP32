#ifndef TYPES_H
#define TYPES_H

// 继电器模式枚举
enum RelayMode {
    MANUAL = 0,
    AUTOMATIC = 1
};

// 校准点结构体
struct CalibrationPoint {
    float voltage;    // 电压值 (0-3.3V)
    float physical;   // 对应的物理量值
};

// ADC校准点结构体
struct ADCCalibPoint {
    float input_voltage;   // 实际输入电压
    float measured_voltage;// ADC测量电压
    float tolerance;       // 容差范围
};

// 模拟量配置结构体
struct AnalogChannel {
    bool enabled;
    String name;
    String unit;
    int gpio;
    int numPoints;
    CalibrationPoint calibPoints[8];
    int filterLimit;      // 限幅值
    float compensation;   // 补偿值
    int lastRawValue;     // 上次的原始值
    unsigned long lastSampleTime; // 上次采样时间
    int sampleBuffer[5];  // 存储5个采样值的缓冲区
    int sampleCount;      // 当前采样计数
    int lastSecondValue;   // 上一秒的中值
    int currentValue;      // 当前使用的值
    int difference;    // 存储当前差值
    static const ADCCalibPoint calibTable[31];  // 添加校准表
};

// 继电器通道结构
struct RelayChannel {
    String name;
    int gpio;
    bool state;
    RelayMode mode;
    bool autoRunning;
    unsigned long onTime;
    unsigned long offTime;
    unsigned int maxCycles;
    unsigned int currentCycles;
    unsigned long lastToggleTime;
};

#endif 