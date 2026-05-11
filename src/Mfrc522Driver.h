#pragma once
#include "IRfidReader.h" 
#include <MFRC522.h>
#include <SPI.h>
#include <array>

class Mfrc522Driver final : public IRfidReader {
private:
    MFRC522 mfrc522;
    const uint8_t ssPin;
    const uint8_t rstPin;
    
    static const std::array<MFRC522::PCD_RxGain, 8> gainLevels;

public:
    explicit Mfrc522Driver(uint8_t ss_pin, uint8_t rst_pin);
    
    bool init() override;
    void setPower(uint8_t level) override;
    bool isCardPresent() override;
    
    // Обновленные методы
    bool readUID(char* buffer, size_t maxLen) override;
    void setAntennaField(bool state) override;
    
    void halt() override;
    void softReset() override;
};