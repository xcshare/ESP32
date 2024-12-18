#ifndef HTML_H
#define HTML_H

#include <Arduino.h>
#include "types.h"  // 包含共享类型定义
#include "ws.h"     // 添加这行，包含 AnalogChannel 定义

// 声明外部变量
extern String systemTitle;
extern RelayChannel relayChannels[4];
extern AnalogChannel analogChannels[12];  // 现在可以识别 AnalogChannel 类型了

// Generate the HTML header section
String generateHeader() {
    return R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset='UTF-8'>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <title>ESP32 控制面板</title>
            <style>
                :root {
                    --primary-color: #2196F3;
                    --hover-color: #1976D2;
                    --active-color: #1565C0;
                    --nav-height: auto;
                }
                
                body { 
                    font-family: Arial, sans-serif; 
                    margin: 0; 
                    padding: 0;
                    background-color: #f5f5f5;
                }
                
                .nav-container {
                    background: white;
                    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
                    position: fixed;
                    top: 0;
                    left: 0;
                    right: 0;
                    z-index: 1000;
                }
                
                .nav { 
                    max-width: 1200px;
                    margin: 0 auto;
                    min-height: 60px;
                    display: flex;
                    align-items: center;
                    padding: 10px 20px;
                    flex-wrap: wrap;
                    gap: 10px;
                }
                
                .nav a { 
                    text-decoration: none;
                    color: #333;
                    padding: 8px 16px;
                    border-radius: 4px;
                    transition: all 0.3s ease;
                    font-weight: 500;
                    position: relative;
                    white-space: nowrap;
                }
                
                .nav a:hover { 
                    background: #f0f0f0;
                    color: var(--primary-color);
                }
                
                .nav a.active {
                    color: var(--primary-color);
                    background: rgba(33, 150, 243, 0.1);
                }
                
                .nav a::after {
                    content: '';
                    position: absolute;
                    bottom: -2px;
                    left: 50%;
                    width: 0;
                    height: 2px;
                    background: var(--primary-color);
                    transition: all 0.3s ease;
                    transform: translateX(-50%);
                }
                
                .nav a:hover::after {
                    width: 100%;
                }
                
                .content { 
                    max-width: 1200px;
                    margin: 90px auto 20px;
                    padding: 20px;
                    background: white;
                    border-radius: 8px;
                    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
                }
                
                @media (max-width: 768px) {
                    .nav {
                        justify-content: center;
                        padding: 5px 10px;
                    }
                    
                    .nav a {
                        flex: 1;
                        min-width: calc(50% - 20px);
                        text-align: center;
                        margin: 5px;
                    }
                    
                    .content {
                        margin: 120px 10px 10px;
                        padding: 15px;
                    }
                }
                .auto-info {
                    font-size: 0.9em;
                    color: #666;
                    margin: 8px 0;
                    padding: 8px;
                    background-color: #f5f5f5;
                    border-radius: 4px;
                    text-align: left;
                    line-height: 1.5;
                }
                .filter-input {
                    width: 100%;
                    padding: 5px;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    box-sizing: border-box;
                }
                .help-text {
                    font-size: 0.85em;
                    color: #666;
                    margin-top: 5px;
                    line-height: 1.4;
                    padding: 5px;
                    background-color: #f8f9fa;
                    border-radius: 4px;
                }
                .compensation-input {
                    width: 100%;
                    padding: 5px;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    box-sizing: border-box;
                }
                
                /* 汉堡菜单按钮样式 */
                .menu-btn {
                    display: none;
                    padding: 10px;
                    background: none;
                    border: none;
                    cursor: pointer;
                    font-size: 1.5em;
                }
                
                /* 移动端样式 */
                @media (max-width: 768px) {
                    .menu-btn {
                        display: block;
                    }
                    
                    .nav {
                        flex-direction: column;
                        align-items: stretch;
                        padding: 0;
                    }
                    
                    .nav-links {
                        display: none;
                        width: 100%;
                        flex-direction: column;
                    }
                    
                    .nav-links.show {
                        display: flex;
                    }
                    
                    .nav-links a {
                        padding: 12px 20px;
                        border-top: 1px solid #eee;
                        text-align: left;
                    }
                    
                    .nav-header {
                        display: flex;
                        justify-content: space-between;
                        align-items: center;
                        padding: 10px 20px;
                    }
                }
            </style>
            <script>
                function toggleMenu() {
                    const navLinks = document.querySelector('.nav-links');
                    navLinks.classList.toggle('show');
                }

                // 点击导链接后自动关闭菜单
                document.querySelectorAll('.nav-links a').forEach(link => {
                    link.addEventListener('click', () => {
                        const navLinks = document.querySelector('.nav-links');
                        navLinks.classList.remove('show');
                    });
                });

                // 点击页面其他地方关闭菜单
                document.addEventListener('click', (e) => {
                    if (!e.target.closest('.nav')) {
                        const navLinks = document.querySelector('.nav-links');
                        navLinks.classList.remove('show');
                    }
                });

                // 添加当前页面高亮
                document.addEventListener('DOMContentLoaded', function() {
                    const path = window.location.pathname;
                    const navLinks = document.querySelectorAll('.nav-links a');
                    navLinks.forEach(link => {
                        if (link.getAttribute('href') === path) {
                            link.classList.add('active');
                        }
                    });
                });
            </script>
        </head>
        <body>
            <div class='nav-container'>
                <div class='nav'>
                    <div class='nav-header'>
                        <span style="COLOR: #FF8C00;">xcshare.site </span>
                        <button class='menu-btn' onclick='toggleMenu()'>☰</button>
                    </div>
                    <div class='nav-links'>
                        <a href='/'>首页</a>
                        <a href='/wifi'>WiFi配置</a>
                        <a href='/analog'>模拟量配置</a>
                        <a href='/relay'>继电器配置</a>
                        <a href='/temp'>温度配置</a>
                    </div>
                </div>
            </div>
            <div class='content'>
    )";
}

// Generate the HTML footer section
String generateFooter() {
    return R"(
            </div>
        </body>
        </html>
    )";
}

// Generate the home page HTML
String generateHomePage() {
    String html = generateHeader();
    html += "<h2>" + systemTitle + "</h2>";
    
    html += R"(
        <style>
            .sensor-container {
                display: grid;
                grid-template-columns: repeat(4, 1fr);
                gap: 20px;
                padding: 20px 0;
            }
            @media (max-width: 768px) {
                .sensor-container {
                    grid-template-columns: repeat(2, 1fr);
                }
            }
            .sensor-card {
                border: 1px solid #ddd;
                border-radius: 8px;
                padding: 15px;
                background-color: #f9f9f9;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .sensor-name {
                font-size: 1.1em;
                font-weight: bold;
                margin-bottom: 10px;
                color: #333;
            }
            .sensor-value {
                font-size: 1.2em;
                color: #2196F3;
            }
            .sensor-details {
                font-size: 0.9em;
                color: #666;
                margin-top: 8px;
            }
            .relay-container {
                display: grid;
                grid-template-columns: repeat(4, 1fr);
                gap: 20px;
                padding: 20px 0;
                margin-top: 20px;
            }
            @media (max-width: 768px) {
                .relay-container {
                    grid-template-columns: repeat(2, 1fr);
                }
            }
            .relay-card {
                border: 1px solid #ddd;
                border-radius: 8px;
                padding: 15px;
                background-color: #f9f9f9;
                text-align: center;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .relay-name {
                font-size: 1.1em;
                font-weight: bold;
                margin-bottom: 10px;
                color: #333;
            }
            .relay-button {
                padding: 10px 20px;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                transition: all 0.3s;
                width: 100%;
                font-size: 1em;
            }
            .relay-on {
                background-color: #4CAF50;
                color: white;
            }
            .relay-off {
                background-color: #f44336;
                color: white;
            }
            .relay-disabled {
                background-color: #ccc;
                cursor: not-allowed;
            }
            .gpio-info {
                font-size: 0.8em;
                color: #666;
                margin-top: 5px;
            }
            .button-group {
                display: flex;
                gap: 10px;
                margin-top: 10px;
            }
            .relay-button, .auto-control-btn {
                flex: 1;
                padding: 10px;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                transition: all 0.3s;
                font-size: 1em;
            }
            .auto-control-btn {
                background-color: #2196F3;
                color: white;
            }
            .auto-control-btn.running {
                background-color: #FFA000;
            }
            .auto-control-btn:disabled {
                background-color: #ccc;
                cursor: not-allowed;
            }
        </style>

        <!-- 传感器数据显示区 -->
        <div id='sensorData' class='sensor-container'></div>

        <!-- 温度传感器显示区域 -->
        <h3>温度监测</h3>
        <div id='tempData' class='sensor-container'>
            <!-- 温度数据将通过WebSocket动态更新 -->
        </div>

        <!-- 继电器控制区域 -->
        <h3>继电器控制</h3>
        <div id='relayControl' class='relay-container'></div>

        <script>
            var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            ws.onmessage = function(event) {
                try {
                    var data = JSON.parse(event.data);
                    
                    // 处理模拟量数据
                    if(data.values) {
                        var container = document.getElementById('sensorData');
                        container.innerHTML = '';
                        
                        data.values.forEach(function(sensor) {
                            var sensorDiv = document.createElement('div');
                            sensorDiv.className = 'sensor-card';
                            
                            // 计算实际的GPIO编号
                            var gpioNum = sensor.channel < 8 ? sensor.channel + 1 : sensor.channel + 7;
                            
                            var html = `
                                <div class="sensor-name">${sensor.name}</div>
                                <div class="sensor-value">${sensor.value.toFixed(2)} ${sensor.unit}</div>
                                <div class="sensor-details">
                                    GPIO${gpioNum}<br>
                                    原始电压: ${sensor.rawVoltage.toFixed(3)}V<br>
                                    校准电压: ${sensor.voltage.toFixed(3)}V<br>
                                    原始值: ${sensor.rawValue}
                                </div>
                            `;
                            
                            sensorDiv.innerHTML = html;
                            container.appendChild(sensorDiv);
                        });
                    }
                    
                    // 处理温度数据
                    if(data.temperatures) {
                        var tempContainer = document.getElementById('tempData');
                        if(tempContainer) {
                            tempContainer.innerHTML = '';
                            
                            // 只显示启用的通道
                            data.temperatures.filter(temp => temp.enabled).forEach(function(temp) {
                                var tempDiv = document.createElement('div');
                                tempDiv.className = 'sensor-card';
                                
                                var html = `
                                    <div class="sensor-name">${temp.name}</div>
                                    <div class="sensor-value">${temp.value.toFixed(2)} °C</div>
                                    <div class="sensor-resistance">电阻: ${temp.resistance.toFixed(2)} Ω</div>
                                    <div class="sensor-details">
                                        类型: ${temp.type === 0 ? 'PT100' : 'PT1000'}<br>
                                        状态: ${temp.fault ? '故障' : '常'}
                                    </div>
                                `;
                                
                                tempDiv.innerHTML = html;
                                tempContainer.appendChild(tempDiv);
                            });

                            // 如果没有启用的通道，显示提示信息
                            if (!data.temperatures.some(temp => temp.enabled)) {
                                var noDataDiv = document.createElement('div');
                                noDataDiv.className = 'no-data-message';
                                noDataDiv.innerHTML = '没有启用的温度传感器';
                                tempContainer.appendChild(noDataDiv);
                            }
                        }
                    }
                } catch(e) {
                    console.error('Error parsing WebSocket message:', e);
                }
            };

            // 处理WebSocket连错误
            ws.onerror = function(error) {
                console.error('WebSocket错误:', error);
            };

            // 处理WebSocket连接关闭
            ws.onclose = function() {
                console.log('WebSocket连接已关闭');
                // 尝试重新连接
                setTimeout(function() {
                    console.log('尝试重新连接...');
                    ws = new WebSocket('ws://' + window.location.hostname + '/ws');
                }, 5000);
            };

            // 更新继电器UI的函数
            function updateRelayUI() {
                fetch('/get_relay_status')
                    .then(function(response) { return response.json(); })
                    .then(function(data) {
                        var container = document.getElementById('relayControl');
                        container.innerHTML = '';
                        
                        data.relays.forEach(function(relay, index) {
                            var div = document.createElement('div');
                            div.className = 'relay-card';
                            
                            // 获取对应的GPIO编号
                            var gpioNum;
                            switch(index) {
                                case 0: gpioNum = 21; break;  // 继电器1 - GPIO21
                                case 1: gpioNum = 45; break;  // 继电器2 - GPIO45
                                case 2: gpioNum = 47; break;  // 继电器3 - GPIO47
                                case 3: gpioNum = 48; break;  // 继电器4 - GPIO48
                                default: gpioNum = relay.gpio; break;
                            }
                            
                            var buttonClass = relay.mode === 0 ? 
                                (relay.state ? 'relay-on' : 'relay-off') : 
                                'relay-disabled';
                            
                            var buttonDisabled = relay.mode === 1 ? ' disabled' : '';
                            var buttonText = relay.state ? '关���' : '启动';
                            
                            div.innerHTML = 
                                "<div class=\"relay-name\">" + relay.name + "</div>" +
                                "<div class=\"gpio-info\">GPIO" + gpioNum + "</div>" +
                                (relay.mode === 1 ? 
                                    "<div class=\"auto-info\">" +
                                    "循环次数: " + (relay.currentCycles || 0) + "/" + (relay.maxCycles || 0) + "<br>" +
                                    "开时间: " + ((relay.onTime || 0)/1000).toFixed(1) + "秒<br>" +
                                    "关位时间: " + ((relay.offTime || 0)/1000).toFixed(1) + "秒" +
                                    "</div>" : "") +
                                "<div class=\"button-group\">" +
                                "<button class=\"relay-button " + buttonClass + "\"" +
                                " onclick=\"toggleRelay(" + index + ")\"" +
                                (relay.mode === 1 ? ' disabled' : '') + ">" +
                                buttonText +
                                "</button>" +
                                (relay.mode === 1 ? 
                                    "<button class=\"auto-control-btn" + (relay.autoRunning ? ' running' : '') + "\"" +
                                    " onclick=\"toggleAutoRun(" + index + ")\">" +
                                    (relay.autoRunning ? '止' : '运行') +
                                    "</button>" : "") +
                                "</div>";
                            
                            container.appendChild(div);
                        });
                    });
            }
            
            // 切换继电器态
            function toggleRelay(channel) {
                var buttons = document.querySelectorAll('.relay-button');
                var button = buttons[channel];
                var newState = button.classList.contains('relay-off') ? 1 : 0;
                
                fetch('/relay/set', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: 'channel=' + channel + '&state=' + newState
                })
                .then(function(response) { return response.text(); })
                .then(function(result) {
                    if(result === 'OK') {
                        updateRelayUI();
                    }
                });
            }
            
            // 页面加载完成后开定时更新
            setInterval(updateRelayUI, 1000);
            updateRelayUI();

            // 在 generateHomePage 添加 toggleAutoRun 函数
            function toggleAutoRun(channel) {
                var buttons = document.querySelectorAll('.auto-control-btn');
                var button = buttons[channel];
                var running = !button.classList.contains('running');
                
                fetch('/relay/auto_control', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: 'channel=' + channel + '&running=' + (running ? '1' : '0')
                })
                .then(function(response) { return response.text(); })
                .then(function(result) {
                    if(result === 'OK') {
                        updateRelayUI();
                    } else {
                        alert('操作失败');
                    }
                })
                .catch(function(error) {
                    console.error('自动控制失败:', error);
                    alert('操作失败');
                });
            }
        </script>
    )";
    html += generateFooter();
    return html;
}

// Generate the WiFi configuration page HTML
String generateWiFiPage(bool connected, String ssid, String ip) {
    String html = generateHeader();
    html += "<h2>系统设置</h2>";
    
    // 添加两列布局的容器
    html += "<div class='settings-container'>";
    
    // 左列：系统标题设置
    html += "<div class='settings-column'>";
    html += "<div class='settings-section'>";
    html += "<h3>系统标题</h3>";
    html += "<div class='title-container'>";
    html += "<input type='text' id='titleInput' value='" + systemTitle + "' class='title-input'>";
    html += "<button onclick='saveTitle()' class='save-btn'>存标题</button>";
    html += "</div>";
    html += "</div>";
    html += "</div>";

    // 右列：WiFi 设置
    html += "<div class='settings-column'>";
    html += "<div class='settings-section'>";
    html += "<h3>WiFi</h3>";
    if(connected) {
        html += "<p>当前连接状态: 已连接</p>";
        html += "<p>连接的WiFi: " + ssid + "</p>";
        html += "<p>IP址: " + ip + "</p>";
    } else {
        html += "<p>当前连接状态: 未连接</p>";
    }
    
    html += R"(
        <form onsubmit='connect(); return false;'>
            <p>
                <label>WiFi名称:</label><br>
                <input type='text' id='ssid' name='ssid'>
            </p>
            <p>
                <label>WiFi密码:</label><br>
                <input type='password' id='pass' name='pass'>
            </p>
            <p>
                <input type='submit' value='连接' class='connect-btn'>
            </p>
        </form>
        </div>
        </div>
        </div>
        <style>
            .settings-container {
                display: grid;
                grid-template-columns: repeat(2, 1fr);
                gap: 20px;
                margin: 0 auto;
                max-width: 1200px;
            }
            @media (max-width: 768px) {
                .settings-container {
                    grid-template-columns: 1fr;
                }
            }
            .settings-column {
                min-width: 0;  /* 止内容溢出 */
            }
            .settings-section {
                margin-bottom: 30px;
                padding: 20px;
                background: #f9f9f9;
                border-radius: 8px;
                box-shadow: 0 1px 3px rgba(0,0,0,0.1);
                height: 100%;
                box-sizing: border-box;
            }
            .title-container {
                display: flex;
                gap: 10px;
                align-items: center;
            }
            .title-input {
                flex: 1;
                padding: 8px;
                font-size: 1.1em;
                border: 1px solid #ddd;
                border-radius: 4px;
            }
            .save-btn, .connect-btn {
                padding: 8px 20px;
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                transition: background-color 0.3s;
            }
            .save-btn:hover, .connect-btn:hover {
                background-color: #45a049;
            }
            input[type="text"], input[type="password"] {
                width: 100%;
                padding: 8px;
                margin: 5px 0;
                border: 1px solid #ddd;
                border-radius: 4px;
                box-sizing: border-box;
            }
            label {
                color: #666;
                font-size: 0.9em;
            }
            h3 {
                margin-top: 0;
                color: #333;
                border-bottom: 1px solid #eee;
                padding-bottom: 10px;
                margin-bottom: 20px;
            }
        </style>
    )";

    // JavaScript 代码持不变
    html += R"(
        <script>
            function connect() {
                var ssid = document.getElementById('ssid').value;
                var pass = document.getElementById('pass').value;
                fetch('/connect?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass))
                    .then(function(response) { return response.text(); })
                    .then(function(result) {
                        if(result === 'OK') {
                            alert('WiFi连接成功！');
                            setTimeout(function() { location.reload(); }, 1000);
                        } else {
                            alert('WiFi连接失败！');
                        }
                    });
            }

            function saveTitle() {
                var newTitle = document.getElementById('titleInput').value;
                if(newTitle.trim() === '') {
                    alert('标题不能为');
                    return;
                }
                
                fetch('/save_title', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: 'title=' + encodeURIComponent(newTitle)
                })
                .then(function(response) { return response.text(); })
                .then(function(result) {
                    if(result === 'OK') {
                        alert('系统标题保存成功！');
                    } else {
                        alert('保存失败');
                    }
                })
                .catch(function(error) {
                    alert('保存失败: ' + error);
                });
            }
        </script>
    )";
    html += generateFooter();
    return html;
}

String generateAnalogConfigPage() {
    String html = generateHeader();
    html += "<h2>模拟量配置</h2>";
    html += R"(
        <style>
            .config-container { 
                max-width: 1400px; 
                margin: 0 auto;
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
                gap: 15px;
                padding: 15px;
            }
            @media (max-width: 768px) {
                .config-container {
                    grid-template-columns: 1fr;  /* 手机端单列显示 */
                    padding: 10px;
                }
                .channel-card {
                    padding: 10px;  /* 减小手机端的内边距 */
                }
                .input-row {
                    flex-direction: row;  /* 持平排列 */
                    flex-wrap: nowrap;    /* 止换行 */
                    gap: 5px;
                }
                .input-row > div {
                    width: 33.33%;        /* 个输入框占三分之一宽度 */
                    min-width: 0;         /* 允许容收缩 */
                }
                .input-row input {
                    padding: 4px 2px;     /* 减小内边距 */
                    font-size: 0.9em;     /* 稍微减小字体 */
                }
                .input-row label {
                    font-size: 0.8em;     /* 减小标签字体 */
                    white-space: nowrap;  /* 防止标签换行 */
                    overflow: hidden;     /* 隐溢出内容 */
                    text-overflow: ellipsis; /* 显示省略号 */
                }
            }
            .channel-card {
                border: 1px solid #ddd;
                border-radius: 8px;
                padding: 15px;
                background-color: #f9f9f9;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .channel-header {
                display: flex;
                align-items: center;
                gap: 10px;
                margin-bottom: 15px;
                padding-bottom: 10px;
                border-bottom: 1px solid #eee;
            }
            .channel-form-group {
                margin-bottom: 8px;
            }
            .input-row {
                display: flex;
                gap: 10px;
                align-items: center;
                margin-bottom: 8px;
            }
            .input-row > div {
                flex: 1;
            }
            .input-row label {
                display: block;
                margin-bottom: 3px;
                font-size: 0.9em;
            }
            .input-row input {
                width: 100%;
                padding: 4px;
                border: 1px solid #ddd;
                border-radius: 4px;
                box-sizing: border-box;
            }
            .help-text {
                font-size: 0.85em;
                color: #666;
                margin: 3px 0;
                line-height: 1.2;
            }
            .diff-value {
                display: block;
                padding: 4px 0;
            }
            .voltage-note {
                color: #666;
                font-size: 0.85em;
                margin: 5px 0;
                padding: 5px;
                background-color: #fff3cd;
                border-radius: 4px;
                border: 1px solid #ffeeba;
            }
            .save-btn, .expand-btn {
                padding: 8px 16px;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                font-size: 0.9em;
                transition: background-color 0.3s;
            }
            .save-btn {
                background-color: #4CAF50;
            }
            .expand-btn {
                background-color: #2196F3;
            }
            .save-btn:hover { 
                background-color: #45a049;
            }
            .expand-btn:hover { 
                background-color: #1976D2;
            }
            .channel-title {
                font-size: 1.1em;
                font-weight: bold;
                color: #333;
            }
            .extra-points {
                display: none;
            }
            .extra-points.show {
                display: table-row;
            }
            .button-group {
                display: flex;
                gap: 10px;
                margin-top: 15px;
                justify-content: space-between;
            }
            .input-group {
                display: flex;
                gap: 5px;
            }
            .input-group input {
                flex: 1;
            }
            .filter-input-group {
                display: flex;
                align-items: center;
                gap: 10px;
            }
            .filter-input {
                flex: 1;
            }
            .diff-value {
                color: #666;
                font-size: 0.9em;
                white-space: nowrap;
            }
        </style>
        <div class="config-container">
    )";

    // 生成12个通道的配置卡片
    for (int i = 0; i < 12; i++) {  // 从8改为12
        // 计算实际的GPIO编号
        int gpioNum = i < 8 ? i + 1 : i + 7;  // GPIO1-8, GPIO15-18
        
        html += "<div class='channel-card'>";
        html += "<div class='channel-header'>";
        html += "<input type='checkbox' id='enable" + String(i) + "' checked>";
        html += "<span class='channel-title'>通道 " + String(i + 1) + " (GPIO" + String(gpioNum) + ")</span>";
        html += "</div>";
        
        // 名称单独一行
        html += "<div class='channel-form-group'>";
        html += "<label>名称:</label>";
        html += "<input type='text' id='name" + String(i) + "' placeholder='传感器 " + String(i + 1) + "'>";
        html += "</div>";
        
        // 单位限幅值和补偿值在同一行
        html += "<div class='input-row'>";
        // 单位
        html += "<div>";
        html += "<label>单位:</label>";
        html += "<input type='text' id='unit" + String(i) + "' placeholder='单'>";
        html += "</div>";
        // 限幅值
        html += "<div>";
        html += "<label>限幅滤波值:</label>";
        html += "<input type='number' id='filter" + String(i) + "' min='0' max='200' value='20' class='filter-input' onchange='updateFilterLimit(" + String(i) + ")'>";
        html += "</div>";
        // 补偿值
        html += "<div>";
        html += "<label>补偿值:</label>";
        html += "<input type='number' id='comp" + String(i) + "' step='0.1' value='0' class='compensation-input'>";
        html += "</div>";
        html += "</div>";

        // 限幅值范围说明单独一行
        html += "<div class='help-text'>限幅滤波范围：0-200，默认：20。当前值与上次值之差大于此值时才更新，建议根据实波动情况调整</div>";

        // 差值显示独一行
        html += "<div class='diff-value' id='diff" + String(i) + "'>当前差值: 0</div>";

        // 压值范围提示
        html += "<div class='voltage-note'>⚠️ 电压值范围：0-3.0V (注意：3.0V以上为ADC死区)</div>";
        
        // 校准点表格
        html += "<table class='calibration-table'>";
        html += "<tr><th width='20%'>点</th><th width='40%'>电压 (V)</th><th width='40%'>物理量</th></tr>";

        // 显示前两个校准点
        for (int j = 0; j < 2; j++) {
            html += "<tr>";
            html += "<td>" + String(j + 1) + "</td>";
            html += "<td><input type='number' id='v" + String(i) + "_" + String(j) + 
                    "' step='0.1' min='0' max='3.0' placeholder='0-3.0'></td>";
            html += "<td><input type='number' id='p" + String(i) + "_" + String(j) + 
                    "' step='0.1' placeholder='值'></td>";
            html += "</tr>";
        }

        // 额外的6个校准点（默认隐藏）
        for (int j = 2; j < 8; j++) {
            html += "<tr class='extra-points' id='extra" + String(i) + "'>";
            html += "<td>" + String(j + 1) + "</td>";
            html += "<td><input type='number' id='v" + String(i) + "_" + String(j) + 
                    "' step='0.1' min='0' max='3.0' placeholder='0-3.0'></td>";
            html += "<td><input type='number' id='p" + String(i) + "_" + String(j) + 
                    "' step='0.1' placeholder='值'></td>";
            html += "</tr>";
        }
        
        html += "</table>";
        
        // 按钮组
        html += "<div class='button-group'>";
        html += "<button type='button' class='expand-btn' onclick='toggleExtraPoints(" + String(i) + ")'>更多校准点</button>";
        html += "<button type='button' class='save-btn' onclick='saveChannelConfig(" + String(i) + ")'>保存配置</button>";
        html += "</div>";
        
        html += "</div>";
    }

    html += "</div>";  // 结束 config-container

    // 添加接线指导说明
    html += R"(
        <div class="wiring-guide">
            <h3>接线指导说明</h3>
            
            <!-- 添加GPIO分配说明 -->
            <div class="gpio-allocation">
                <h4>GPIO通道分配:</h4>
                <table class="gpio-table">
                    <tr><th>通道号</th><th>GPIO</th></tr>
                    <tr><td>通道1-8</td><td>GPIO1-8</td></tr>
                    <tr><td>通道9</td><td>GPIO15</td></tr>
                    <tr><td>通道10</td><td>GPIO16</td></tr>
                    <tr><td>通道11</td><td>GPIO17</td></tr>
                    <tr><td>通道12</td><td>GPIO18</td></tr>
                </table>
                <p class="note">注意：GPIO9-14未使用请勿接线到这些引脚。</p>
            </div>

            <style>
                .wiring-guide {
                    margin-top: 30px;
                    padding: 20px;
                    background: #f8f9fa;
                    border-radius: 8px;
                    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
                }
                .wiring-table {
                    width: 100%;
                    border-collapse: collapse;
                    margin: 20px 0;
                }
                .wiring-table th, .wiring-table td {
                    padding: 12px;
                    border: 1px solid #ddd;
                    text-align: left;
                }
                .wiring-table th {
                    background: #f5f5f5;
                }
                .wiring-diagram {
                    margin: 20px 0;
                    padding: 20px;
                    background: white;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                }
                .diagram-container {
                    display: flex;
                    gap: 20px;
                    flex-wrap: wrap;
                }
                .diagram-box {
                    flex: 1;
                    min-width: 300px;
                    padding: 15px;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    background: white;
                }
                .diagram-title {
                    font-weight: bold;
                    margin-bottom: 10px;
                    color: #333;
                }
                pre {
                    background: #f5f5f5;
                    padding: 10px;
                    border-radius: 4px;
                    overflow-x: auto;
                }
            </style>
            
            <table class="wiring-table">
                <tr>
                    <th>输入信号</th>
                    <th>分压电阻</th>
                    <th>转换后电压范围</th>
                </tr>
                <tr>
                    <td>1-5V</td>
                    <td>R1=10kΩ, R2=25kΩ</td>
                    <td>0.714-3.0V</td>
                </tr>
                <tr>
                    <td>0-5V</td>
                    <td>R1=10kΩ, R2=27kΩ</td>
                    <td>0-3.0V</td>
                </tr>
                <tr>
                    <td>4-20mA</td>
                    <td>R=150Ω</td>
                    <td>0.6-3.0V</td>
                </tr>
            </table>

            <div class="diagram-container">
                <div class="diagram-box">
                    <div class="diagram-title">电压信号接线图 (0-5V/1-4.8V)</div>
                    <pre>
输入信号 ----[R1]----+----[R2]----GND
                     |
                     |
                    ADC
                    </pre>
                    <p>说明：</p>
                    <ul>
                        <li>R1和R2分压</li>
                        <li>ADC采样点接R1和R2的点</li>
                        <li>GND接地端</li>
                    </ul>
                </div>

                <div class="diagram-box">
                    <div class="diagram-title">电流信号接线图 (4-20mA)</div>
                    <pre>
电流信号 ----[R=150Ω]----GND
                      |
                      |
                     ADC
                    </pre>
                    <p>说明：</p>
                    <ul>
                        <li>采用150Ω精密��阻</li>
                        <li>ADC采样点接电阻端</li>
                        <li>电阻另一端接地</li>
                    </ul>
                </div>
            </div>

            <div class="wiring-notes">
                <h4>注意事项：</h4>
                <ul>
                    <li>请使用精密电阻以确保测精度</li>
                    <li>确保线牢固，避免虚接</li>
                    <li>接线注意信号极性</li>
                    <li>建议在输入端增加保护电路</li>
                </ul>
            </div>
        </div>
    )";

    // 在 generateAnalogConfigPage 函数中，在接线指导说明添加
    html += R"(
        <script>
            // 全局 WebSocket 变量
            var ws;
            
            // WebSocket 连函数
            function initWebSocket() {
                if (!ws || ws.readyState !== WebSocket.OPEN) {
                    ws = new WebSocket('ws://' + window.location.hostname + '/ws');
                    ws.onmessage = function(event) {
                        try {
                            var data = JSON.parse(event.data);
                            if(data.values) {
                                data.values.forEach(function(channel) {
                                    // 更新差值显示
                                    var diffSpan = document.getElementById('diff' + channel.channel);
                                    if(diffSpan) {
                                        diffSpan.textContent = '当前差值: ' + channel.difference;
                                        console.log('Channel ' + channel.channel + ' difference: ' + channel.difference);
                                    }
                                });
                            }
                        } catch(e) {
                            console.error('Error parsing WebSocket message:', e);
                        }
                    };
                    
                    ws.onclose = function() {
                        console.log('WebSocket disconnected');
                        setTimeout(initWebSocket, 2000);
                    };
                }
            }

            // 加载配置函数
            function loadConfig() {
                fetch('/get_analog_config')
                    .then(response => response.json())
                    .then(data => {
                        data.channels.forEach((channel, i) => {
                            document.getElementById('enable' + i).checked = channel.enabled;
                            document.getElementById('name' + i).value = channel.name;
                            document.getElementById('unit' + i).value = channel.unit;
                            document.getElementById('filter' + i).value = channel.filterLimit;
                            document.getElementById('comp' + i).value = channel.compensation || 0;
                            
                            // 填充校准点数据
                            channel.calibPoints.forEach((point, j) => {
                                if (point.voltage !== undefined) {
                                    document.getElementById('v' + i + '_' + j).value = point.voltage;
                                    document.getElementById('p' + i + '_' + j).value = point.physical;
                                }
                            });
                        });
                    })
                    .catch(error => {
                        console.error('Error loading config:', error);
                    });
            }

            // 修改保存通道配置函数
            function saveChannelConfig(channelIndex) {
                var config = [];
                var channelConfig = {
                    channel: channelIndex,
                    enabled: document.getElementById('enable' + channelIndex).checked,
                    name: document.getElementById('name' + channelIndex).value || '传感器 ' + (channelIndex + 1),
                    unit: document.getElementById('unit' + channelIndex).value || '单位',
                    filterLimit: parseInt(document.getElementById('filter' + channelIndex).value) || 20,
                    compensation: parseFloat(document.getElementById('comp' + channelIndex).value) || 0,
                    calibPoints: []
                };

                // 收集所有有效的校准点
                for (var j = 0; j < 8; j++) {
                    var voltage = parseFloat(document.getElementById('v' + channelIndex + '_' + j).value);
                    var physical = parseFloat(document.getElementById('p' + channelIndex + '_' + j).value);
                    if (!isNaN(voltage) && !isNaN(physical)) {
                        channelConfig.calibPoints.push({
                            voltage: voltage,
                            physical: physical
                        });
                    }
                }

                config.push(channelConfig);

                fetch('/save_analog_config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({config: config})
                })
                .then(response => response.text())
                .then(result => {
                    alert('通道 ' + (channelIndex + 1) + ' 配置已保存');
                    loadConfig();  // 重新加载配置以验证
                })
                .catch(error => {
                    console.error('Save error:', error);
                    loadConfig();  // 即使出错也重新加载配置
                });
            }

            // 切换显示额外的校准点
            function toggleExtraPoints(channelIndex) {
                const extraPoints = document.querySelectorAll('#extra' + channelIndex);
                const btn = event.target;
                let isShowing = extraPoints[0].classList.contains('show');
                
                extraPoints.forEach(row => {
                    row.classList.toggle('show');
                });
                
                btn.textContent = isShowing ? '显示更多校准点' : '隐藏额外校准点';
            }

            // 修改更新限幅值的函数
            function updateFilterLimit(channelIndex) {
                var limit = parseInt(document.getElementById('filter' + channelIndex).value) || 20;
                
                // 添加范围验证
                if (limit < 0) limit = 0;
                if (limit > 200) limit = 200;
                document.getElementById('filter' + channelIndex).value = limit;
                
                fetch('/save_filter_limit', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: 'channel=' + channelIndex + '&limit=' + limit
                })
                .then(response => response.text())
                .then(result => {
                    if(result !== 'OK') {
                        console.error('Failed to update filter limit');
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                });
            }

            // 页面加载时初始化
            document.addEventListener('DOMContentLoaded', function() {
                initWebSocket();
                loadConfig();
            });

            // 页面关闭时清理 WebSocket 连接
            window.addEventListener('beforeunload', function() {
                if (ws) {
                    ws.close();
                }
            });
        </script>
    )";

    html += generateFooter();
    return html;
}

String generateRelayConfigPage() {
    String html = generateHeader();
    html += R"(
        <h2>继电器配置</h2>
        <style>
            .config-container { 
                max-width: 1400px; 
                margin: 0 auto;
                display: grid;
                grid-template-columns: repeat(4, 1fr);
                gap: 15px;
                padding: 15px;
            }
            @media (max-width: 768px) {
                .config-container {
                    grid-template-columns: repeat(2, 1fr);
                }
            }
            .relay-card {
                border: 1px solid #ddd;
                border-radius: 8px;
                padding: 15px;
                background-color: #f9f9f9;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .relay-header {
                display: flex;
                align-items: center;
                gap: 10px;
                margin-bottom: 15px;
                padding-bottom: 10px;
                border-bottom: 1px solid #eee;
            }
            .relay-title {
                font-size: 1.1em;
                font-weight: bold;
                color: #333;
            }
            .form-group {
                margin-bottom: 15px;
            }
            .form-group label {
                display: block;
                margin-bottom: 5px;
                color: #666;
            }
            input[type="text"], select {
                width: 100%;
                padding: 8px;
                border: 1px solid #ddd;
                border-radius: 4px;
                box-sizing: border-box;
            }
            select {
                background-color: white;
            }
            .save-btn {
                width: 100%;
                padding: 10px;
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                transition: background-color 0.3s;
            }
            .save-btn:hover {
                background-color: #45a049;
            }
            .gpio-note {
                font-size: 0.85em;
                color: #666;
                margin-top: 5px;
            }
        </style>
        <div class="config-container">
    )";

    for(int i = 0; i < 4; i++) {
        // 获取对应的GPIO编号
        int gpioNum;
        switch(i) {
            case 0: gpioNum = 21; break;  // 继电器1 - GPIO21
            case 1: gpioNum = 45; break;  // 继电器2 - GPIO45
            case 2: gpioNum = 47; break;  // 继电器3 - GPIO47
            case 3: gpioNum = 48; break;  // 继电器4 - GPIO48
        }

        html += "<div class='relay-card'>";
        html += "<div class='relay-header'>";
        html += "<span class='relay-title'>继电器 " + String(i + 1) + "</span>";
        html += "</div>";
        
        html += "<div class='form-group'>";
        html += "<label>名称:</label>";
        html += "<input type='text' id='name" + String(i) + "' placeholder='继电器 " + String(i + 1) + "'>";
        html += "<div class='gpio-note'>GPIO" + String(gpioNum) + "</div>";
        html += "</div>";
        
        html += "<div class='form-group'>";
        html += "<label>运行模式:</label>";
        html += "<select id='mode" + String(i) + "' onchange='toggleAutoSettings(" + String(i) + ")'>";
        html += "<option value='0'>手动</option>";
        html += "<option value='1'>自动</option>";
        html += "</select>";
        html += "</div>";
        
        html += "<div id='autoSettings" + String(i) + "' class='auto-settings' style='display:none;'>";
        html += "<div class='form-group'>";
        html += "<label>开位停留时间 (秒):</label>";
        html += "<input type='number' id='onTime" + String(i) + "' min='1' step='1'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>关位停停留时间 (秒):</label>";
        html += "<input type='number' id='offTime" + String(i) + "' min='1' step='1'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>循环次数:</label>";
        html += "<input type='number' id='maxCycles" + String(i) + "' min='1' step='1'>";
        html += "</div>";
        html += "</div>";
        
        html += "<button class='save-btn' onclick='saveConfig(" + String(i) + ")'>保存配置</button>";
        html += "</div>";
    }

    html += R"(
        </div>
        <script>
            function toggleAutoSettings(index) {
                var mode = document.getElementById('mode' + index).value;
                var autoSettings = document.getElementById('autoSettings' + index);
                autoSettings.style.display = mode === '1' ? 'block' : 'none';
            }

            // 修改保存配置函数
            function saveConfig(index) {
                var mode = parseInt(document.getElementById('mode' + index).value);
                var config = [{
                    channel: index,
                    name: document.getElementById('name' + index).value || '继电器 ' + (index + 1),
                    mode: mode,
                    onTime: mode === 1 ? parseInt(document.getElementById('onTime' + index).value) * 1000 : 0,
                    offTime: mode === 1 ? parseInt(document.getElementById('offTime' + index).value) * 1000 : 0,
                    maxCycles: mode === 1 ? parseInt(document.getElementById('maxCycles' + index).value) : 0
                }];
                
                fetch('/save_relay_config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({config: config})
                })
                .then(function(response) { return response.text(); })
                .then(function(result) {
                    alert('继电器 ' + (index + 1) + ' 配置已保存');
                })
                .catch(function(error) {
                    alert('保存失败: ' + error);
                });
            }

            // 加载配置时也加载动模式参数
            fetch('/get_relay_config')
                .then(function(response) { return response.json(); })
                .then(function(data) {
                    data.relays.forEach(function(relay, i) {
                        document.getElementById('name' + i).value = relay.name;
                        document.getElementById('mode' + i).value = relay.mode;
                        if(relay.mode === 1) {
                            document.getElementById('onTime' + i).value = relay.onTime / 1000;
                            document.getElementById('offTime' + i).value = relay.offTime / 1000;
                            document.getElementById('maxCycles' + i).value = relay.maxCycles;
                            toggleAutoSettings(i);
                        }
                    });
                });
        </script>
    )";
    html += generateFooter();
    return html;
}

// 在文末尾添加温度配置页面生成函数
String generateTempConfigPage() {
    String html = generateHeader();
    html += R"(
        <h2>温度传感器配置</h2>
        <style>
            .temp-container {
                max-width: 1200px;
                margin: 0 auto;
                display: grid;
                grid-template-columns: repeat(3, 1fr);
                gap: 20px;
                padding: 20px;
            }
            @media (max-width: 768px) {
                .temp-container {
                    grid-template-columns: 1fr;
                }
            }
            .temp-card {
                border: 1px solid #ddd;
                border-radius: 8px;
                padding: 20px;
                background-color: #f9f9f9;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .form-group {
                margin-bottom: 15px;
            }
            .form-group label {
                display: block;
                margin-bottom: 5px;
                color: #666;
            }
            input[type="text"], select {
                width: 100%;
                padding: 8px;
                border: 1px solid #ddd;
                border-radius: 4px;
                box-sizing: border-box;
            }
            .save-btn {
                width: 100%;
                padding: 10px;
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                transition: background-color 0.3s;
            }
            .save-btn:hover {
                background-color: #45a049;
            }
            .pin-info {
                margin-top: 15px;
                padding: 10px;
                background-color: #fff3cd;
                border: 1px solid #ffeeba;
                border-radius: 4px;
                font-size: 0.9em;
            }
        </style>
        
        <div class="temp-container">
    )";

    // 生成两个温度传感器的配置卡片
    for(int i = 0; i < 2; i++) {
        // 获取对应的 CS 引脚
        int csPin = (i == 0) ? 10 : 39;  // 传感器1用GPIO10，传感器2用GPIO39
        
        html += "<div class='temp-card'>";
        html += "<h3>温度传感器 " + String(i + 1) + " (CS" + String(i + 1) + ": GPIO" + String(csPin) + ")</h3>";
        
        html += "<div class='form-group'>";
        html += "<label>启用:</label>";
        html += "<input type='checkbox' id='enable" + String(i) + "'>";
        html += "</div>";
        
        html += "<div class='form-group'>";
        html += "<label>名称:</label>";
        html += "<input type='text' id='name" + String(i) + "' placeholder='温度传感器 " + String(i + 1) + "'>";
        html += "</div>";
        
        html += "<div class='form-group'>";
        html += "<label>传感器类型:</label>";
        html += "<select id='type" + String(i) + "'>";
        html += "<option value='0'>PT100</option>";
        html += "<option value='1'>PT1000</option>";
        html += "</select>";
        html += "</div>";
        
        html += "<button class='save-btn' onclick='saveTempConfig(" + String(i) + ")'>保存配置</button>";
        
        html += "<div class='pin-info'>";
        html += "<h4>接线说明:</h4>";
        html += "<table class='wiring-table'>";
        html += "<tr><th>信号</th><th>GPIO</th><th>说明</th></tr>";
        html += "<tr><td>VIN</td><td>3.3V</td><td>供电电压</td></tr>";
        html += "<tr><td>GND</td><td>GND</td><td>接地</td></tr>";
        html += "<tr><td>SCK</td><td>GPIO12</td><td>SPI时钟信号</td></tr>";
        html += "<tr><td>SDO</td><td>GPIO13</td><td>SPI数据输出(MISO)</td></tr>";
        html += "<tr><td>SDI</td><td>GPIO11</td><td>SPI数据输入(MOSI)</td></tr>";
        html += "<tr><td>CS" + String(i + 1) + "</td><td>GPIO" + String(csPin) + "</td><td>传感器" + String(i + 1) + "片选</td></tr>";
        html += "</table>";
        
        // 添加接线注意事项
        html += "<div class='wiring-notes'>";
        html += "<h4>注意事项:</h4>";
        html += "<ul>";
        html += "<li>所有传感器均使用2线连接方式</li>";
        html += "<li>PT100和PT1000使用相同的接线方式，仅在配置中选择对应类型</li>";
        html += "<li>确保电源电压稳定在3.3V</li>";
        html += "<li>注意信号线不要接错，特是SDI和SDO</li>";
        html += "<li>每个传感器的CS信号线要接到对应的GPIO口</li>";
        html += "</ul>";
        html += "</div>";
        
        // 添加样式
        html += R"(
            <style>
                .wiring-table {
                    width: 100%;
                    border-collapse: collapse;
                    margin: 10px 0;
                }
                .wiring-table th, .wiring-table td {
                    border: 1px solid #ddd;
                    padding: 8px;
                    text-align: left;
                }
                .wiring-table th {
                    background-color: #f5f5f5;
                }
                .wiring-notes {
                    margin-top: 15px;
                }
                .wiring-notes ul {
                    padding-left: 20px;
                }
                .wiring-notes li {
                    margin: 5px 0;
                    color: #666;
                }
            </style>
        )";
        
        html += "</div>";  // 结束 pin-info div
        
        html += "</div>";
    }

    html += R"(
        </div>
        <script>
            // 保存配置
            function saveTempConfig(index) {
                const config = {
                    enabled: document.getElementById('enable' + index).checked,
                    name: document.getElementById('name' + index).value || '温度传感器 ' + (index + 1),
                    type: parseInt(document.getElementById('type' + index).value)
                };

                console.log('Saving config:', JSON.stringify({
                    index: index,
                    config: config
                }));

                fetch('/save_temp_config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({
                        index: index,
                        config: config
                    })
                })
                .then(response => {
                    console.log('Response status:', response.status);
                    if (!response.ok) {
                        return response.text().then(text => {
                            throw new Error(text || 'Network response was not ok');
                        });
                    }
                    return response.text();
                })
                .then(result => {
                    console.log('Save result:', result);
                    if (result === 'OK') {
                        alert('配置已保存');
                        loadTempConfig();
                    } else {
                        throw new Error('Server response: ' + result);
                    }
                })
                .catch(error => {
                    console.error('Save error:', error);
                    alert('保存失败: ' + error.message);
                });
            }

            // 加载配置
            function loadTempConfig() {
                fetch('/get_temp_config')
                    .then(response => {
                        if (!response.ok) {
                            throw new Error('Network response was not ok');
                        }
                        return response.json();
                    })
                    .then(data => {
                        console.log('Loaded config:', data);
                        data.sensors.forEach((sensor, i) => {
                            document.getElementById('enable' + i).checked = sensor.enabled;
                            document.getElementById('name' + i).value = sensor.name;
                            document.getElementById('type' + i).value = sensor.type;
                        });
                    })
                    .catch(error => {
                        console.error('Load error:', error);
                        alert('加载配置失: ' + error.message);
                    });
            }

            // 页面加载时加载配置
            document.addEventListener('DOMContentLoaded', loadTempConfig);
        </script>
    )";
    
    html += generateFooter();
    return html;
}

#endif