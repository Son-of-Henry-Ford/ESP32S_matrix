#include "MatrixManager.h"

MatrixManager::MatrixManager(uint8_t r0, uint8_t r1, uint8_t r2, 
                             uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t en) 
    : enablePin(en) {
    rowPins[0] = r0; rowPins[1] = r1; rowPins[2] = r2;
    colPins[0] = c0; colPins[1] = c1; colPins[2] = c2; colPins[3] = c3;
}

void MatrixManager::init() {
    pinMode(enablePin, OUTPUT);
    digitalWrite(enablePin, HIGH); // Глушим всё сразу
    
    for (uint8_t pin : rowPins) pinMode(pin, OUTPUT);
    for (uint8_t pin : colPins) pinMode(pin, OUTPUT);
}

void MatrixManager::selectAntenna(uint8_t id) {
    if (id >= 96) return;

    const uint8_t muxIndex = id / 8; 
    const uint8_t channel = id % 8;  

    digitalWrite(enablePin, HIGH); // Защита от наводок при переключении
    
    for (uint8_t i = 0; i < ROW_PINS_COUNT; ++i) {
        digitalWrite(rowPins[i], (channel >> i) & 0x01);
    }

    for (uint8_t i = 0; i < COL_PINS_COUNT; ++i) {
        digitalWrite(colPins[i], (muxIndex >> i) & 0x01);
    }

    delayMicroseconds(5); // Даем время на стабилизацию уровней напряжения
    digitalWrite(enablePin, LOW); 
}