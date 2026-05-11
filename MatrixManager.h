#pragma once
#include <Arduino.h>

class MatrixManager {
private:
    // Количество УПРАВЛЯЮЩИХ ПИНОВ, а не каналов
    static constexpr uint8_t ROW_PINS_COUNT = 3; // 2^3 = 8 каналов (мультиплексор)
    static constexpr uint8_t COL_PINS_COUNT = 4; // 2^4 = 16 выходов (дешифратор)
    
    uint8_t rowPins[ROW_PINS_COUNT]; 
    uint8_t colPins[COL_PINS_COUNT];
    uint8_t enablePin;

public:
    explicit MatrixManager(uint8_t r0, uint8_t r1, uint8_t r2, 
                           uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t en);
    
    void init();
    void selectAntenna(uint8_t id);
};