#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <string>
#include <random>

// 颜色代码，增加视觉趣味性
const std::string GREEN = "\033[32m";
const std::string RED   = "\033[31m";
const std::string CYAN  = "\033[36m";
const std::string RESET = "\033[0m";

// 全局资源
int firewall_hp = 1000;          // 防火墙总生命值
std::mutex mtx;                 // 保护生命值的锁
std::atomic<bool> system_cracked{false}; // 系统是否被攻破

// 模拟黑客攻击函数
void hacker_attack(int id, int power) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<> sleep_dist(100, 500);

    while (firewall_hp > 0) {
        // 模拟攻击准备时间
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));

        // 尝试锁定并修改共享资源
        std::lock_guard<std::mutex> lock(mtx);

        if (firewall_hp <= 0) break;

        firewall_hp -= power;
        std::cout << CYAN << "[Hacker #" << id << "]" << RESET
                  << " 注入恶意封包... 造成 " << power << " 点伤害! "
                  << "剩余防御力: " << firewall_hp << std::endl;

        if (firewall_hp <= 0) {
            system_cracked = true;
            std::cout << RED << "!!! [Hacker #" << id << "] 成功上传病毒，系统崩溃 !!!" << RESET << std::endl;
        }
    }
}

// 模拟系统自愈函数 (深度：后台干扰线程)
void system_auto_repair() {
    while (!system_cracked) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        std::lock_guard<std::mutex> lock(mtx);
        if (firewall_hp > 0 && firewall_hp < 1000) {
            firewall_hp += 20; // 每秒恢复20点
            std::cout << GREEN << "[System] 检测到入侵... 启动自愈程序 (+20 HP)" << RESET << std::endl;
        }
    }
}

int main() {
    std::cout << "--- 正在连接到非法网关 ---" << std::endl;
    std::vector<std::thread> hackers;

    // 1. 启动防火墙自愈线程 (后台线程)
    std::thread repair_service(system_auto_repair);

    // 2. 启动多个黑客线程 (并发攻击)
    for (int i = 1; i <= 3; ++i) {
        hackers.emplace_back(hacker_attack, i, 50 + (i * 10));
    }

    // 3. 等待黑客们完成任务
    for (auto& t : hackers) {
        if (t.joinable()) t.join();
    }

    // 4. 等待自愈线程结束 (由于原子变量，它会自动停止)
    if (repair_service.joinable()) repair_service.join();

    std::cout << "--- 任务完成，数据已导出 ---" << std::endl;
    return 0;
}