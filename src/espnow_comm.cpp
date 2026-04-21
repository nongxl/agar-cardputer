#include "espnow_comm.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len);

static MessageCallback _callback = nullptr;

void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (_callback) {
        _callback(mac_addr, data, len);
    }
}

EspNowComm::EspNowComm() {}

bool EspNowComm::init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    if (esp_now_init() != ESP_OK) {
        return false;
    }

    esp_now_register_recv_cb(onDataRecv);
    
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (!esp_now_is_peer_exist(broadcastAddress)) {
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        
        // [V14] 通道锁定策略：优先使用 STA 当前通道
        peerInfo.channel = WiFi.channel();
        if (peerInfo.channel == 0) peerInfo.channel = 1; 
        peerInfo.encrypt = false;
        
        esp_now_add_peer(&peerInfo);
    }
    
    return true;
}

void EspNowComm::setNotifyCallback(MessageCallback cb) {
    _callback = cb;
}

void EspNowComm::broadcast(const uint8_t* data, size_t len) {
    // [V14 CRITICAL] 长度熔断保护
    if (len > 250) return; 

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastAddress, data, len);
}

void EspNowComm::sendTo(const uint8_t* mac, const uint8_t* data, size_t len) {
    if (len > 250) return;

    // [V54] 动态 Peer 注册：如果该地址不在列表里，先添加
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = WiFi.channel();
        if (peerInfo.channel == 0) peerInfo.channel = 1;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    esp_now_send(mac, data, len);
}
