#include "Mfrc522Driver.h"
#include <cstdio> // Обязательно для snprintf

// Читаемый массив уровней усиления (Gain)
const std::array<MFRC522::PCD_RxGain, 8> Mfrc522Driver::gainLevels = {
    MFRC522::RxGain_18dB,    // 0: 18 dB
    MFRC522::RxGain_23dB,    // 1: 23 dB  
    MFRC522::RxGain_18dB_2,  // 2: 18 dB
    MFRC522::RxGain_23dB_2,  // 3: 23 dB
    MFRC522::RxGain_33dB,    // 4: 33 dB
    MFRC522::RxGain_38dB,    // 5: 38 dB
    MFRC522::RxGain_43dB,    // 6: 43 dB
    MFRC522::RxGain_48dB     // 7: 48 dB
};

Mfrc522Driver::Mfrc522Driver(uint8_t ss_pin, uint8_t rst_pin) 
    : ssPin(ss_pin), rstPin(rst_pin), mfrc522(ss_pin, rst_pin) {}

bool Mfrc522Driver::init() {
    mfrc522.PCD_Init();
    bool isOk = mfrc522.PCD_PerformSelfTest();
    mfrc522.PCD_Init(); 
    
    // Выключаем поле по умолчанию, чтобы не греть чип вхолостую
    setAntennaField(false);
    return isOk;
}

void Mfrc522Driver::setPower(uint8_t level) {
    uint8_t safeLevel = (level > 7) ? 7 : level;
    mfrc522.PCD_SetAntennaGain(gainLevels[safeLevel]);
}

bool Mfrc522Driver::isCardPresent() {
    return mfrc522.PICC_IsNewCardPresent();
}

// O(1) Memory Allocation - используем переданный буфер (C-строку)
bool Mfrc522Driver::readUID(char* buffer, size_t maxLen) {
    if (!mfrc522.PICC_ReadCardSerial()) return false;
    
    uint8_t size = mfrc522.uid.size;
    if (size > 10) size = 10; 
    
    // Проверка, влезет ли UID (2 символа на байт + null-терминатор)
    if (maxLen < (size * 2 + 1)) return false;

    int offset = 0;
    for (byte i = 0; i < size; i++) {
        // Безопасно пишем по 2 hex-символа в буфер
        offset += snprintf(buffer + offset, maxLen - offset, "%02X", mfrc522.uid.uidByte[i]);
    }
    
    return true;
}

// Защита аналогового тракта
void Mfrc522Driver::setAntennaField(bool state) {
    if (state) {
        mfrc522.PCD_AntennaOn();
    } else {
        mfrc522.PCD_AntennaOff();
    }
}

void Mfrc522Driver::halt() {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

void Mfrc522Driver::softReset() {
    mfrc522.PCD_Reset();
    mfrc522.PCD_Init();
}