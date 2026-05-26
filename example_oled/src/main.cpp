/**
 * @file main.cpp
 * @brief UART 示例程序入口
 * @details 演示如何使用 Uart 类进行串口通信
 * @version 1.0.0
 * @date 2026-05-01
 */

#include <print>
#include <filesystem>
#include <algorithm>
#include <array>
#include <string>
#include "HardWare/oled/HAL_OLED.h"
#include "System/SystemState.hpp"

int main() {
    OLED_Init(); 
    OLED_Clear();
    std::print("System Monitor Started...\n");
char buffer[32];
    for(;;){
        std::string timeStr = SystemState::getTime();
        std::string ipStr = SystemState::getIP();
        float cpu = SystemState::getCPUUsage();
        float mem = SystemState::getMemUsage();

        sprintf(buffer, "Time: ");
        OLED_ShowString(0, 0, buffer, OLED_8X16);
        OLED_ShowString(48, 0, (char*)timeStr.c_str(),OLED_8X16);
        sprintf(buffer, "IP: ");
        OLED_ShowString(0, 16, buffer, OLED_8X16);
        OLED_ShowString(32, 16, (char*)ipStr.c_str(),OLED_8X16);
        sprintf(buffer, "CPU: %.1f%%", cpu);
        OLED_ShowString(0, 32, buffer,OLED_8X16);
        sprintf(buffer, "Mem: %.1f%%", mem);
        OLED_ShowString(0, 48, buffer,OLED_8X16);
        OLED_Update();
        usleep(1000000);
    }
    return 0;
}