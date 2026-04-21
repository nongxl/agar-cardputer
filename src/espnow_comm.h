#ifndef ESPNOW_COMM_H
#define ESPNOW_COMM_H

#include <esp_now.h>
#include <WiFi.h>

// [V11] 统一定义接收回调类型
typedef void (*MessageCallback)(const uint8_t* mac_addr, const uint8_t* data, int len);

class EspNowComm {
public:
    EspNowComm();
    bool init();
    
    // 广播消息：支持 size_t 标准长度
    void broadcast(const uint8_t* data, size_t len);
    
    // [V54] 定向发送消息
    void sendTo(const uint8_t* mac, const uint8_t* data, size_t len);
    
    // 设置接收通知回调
    void setNotifyCallback(MessageCallback cb);

private:
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

#endif // ESPNOW_COMM_H
