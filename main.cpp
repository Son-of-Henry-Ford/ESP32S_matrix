#include <Arduino.h>
#include <SPI.h>
#include "Mfrc522Driver.h"
#include "MatrixManager.h"

namespace Config {
    constexpr uint8_t SCK = 12;
    constexpr uint8_t MISO = 13;
    constexpr uint8_t MOSI = 11;
    constexpr uint8_t SS = 4;
    constexpr uint8_t RST = 1;
    
    constexpr uint8_t MATRIX_EN = 15;
    constexpr uint8_t DEC_A0 = 7; 
    constexpr uint8_t DEC_A1 = 6;
    constexpr uint8_t DEC_A2 = 5;
    constexpr uint8_t DEC_A3 = 2; 
    constexpr uint8_t MUX_S0 = 8;
    constexpr uint8_t MUX_S1 = 9;
    constexpr uint8_t MUX_S2 = 10;
    
    // Максимальное количество меток для считывания с одной антенны за раз
    constexpr uint8_t MAX_TAGS_PER_READ = 3; 
}

MatrixManager matrix(Config::MUX_S0, Config::MUX_S1, Config::MUX_S2, 
                     Config::DEC_A0, Config::DEC_A1, Config::DEC_A2, Config::DEC_A3, 
                     Config::MATRIX_EN);
                     
Mfrc522Driver rfidReader(Config::SS, Config::RST);

// Мьютекс для защиты UART 
SemaphoreHandle_t uartMutex;

/**
 * Изолированная функция сканирования одной антенны.
 * ПРОМЫШЛЕННЫЙ ПОДХОД: Потоковая передача данных (Zero-Allocation).
 */
void scanAndReportAntenna(uint8_t id, uint8_t power) {
    // 1. СТРОГО выключаем ВЧ поле перед коммутацией
    rfidReader.setAntennaField(false);
    
    matrix.selectAntenna(id);
    rfidReader.setPower(power);
    
    // 2. Включаем ВЧ поле на новой антенне
    rfidReader.setAntennaField(true);
    
    // Даем 5 мс на раскачку электромагнитного контура (13.56 МГц)
    vTaskDelay(pdMS_TO_TICKS(5)); 
    
    int tagsFound = 0;
    bool prefixPrinted = false;
    char uidBuffer[24]; // Локальный стек, абсолютно никакой кучи (O(1) Memory)
    
    while (tagsFound < Config::MAX_TAGS_PER_READ) {
        // Читаем UID прямо в наш локальный буфер uidBuffer
        if (rfidReader.isCardPresent() && rfidReader.readUID(uidBuffer, sizeof(uidBuffer))) {
            rfidReader.halt(); 
            
            // Если нашли первую метку, печатаем префикс ответа
            if (!prefixPrinted) {
                Serial.printf("RES %d %s", id, uidBuffer);
                prefixPrinted = true;
            } else {
                // Для последующих меток добавляем только пробел и UID
                Serial.printf(" %s", uidBuffer);
            }
            tagsFound++;
        } else {
            // Больше меток в поле антенны нет (или помеха/коллизия разорвала чтение)
            break; 
        }
    }
    
    // Выключаем поле сразу после чтения
    rfidReader.setAntennaField(false);
    
    // Если была найдена хотя бы одна метка, закрываем строку символом переноса
    if (prefixPrinted) {
        Serial.println();
    }
}

/**
 * Задача обработки команд от сервера (Orange Pi).
 */
void TaskUART(void *pvParameters) {
    char rxBuffer[128];
    uint8_t rxIndex = 0;

    for (;;) {
        uint8_t bytesProcessed = 0;
        
        // Читаем не более 64 байт за один проход (защита от UART-флуда и зависания ядра)
        while (Serial.available() > 0 && bytesProcessed++ < 64) {
            char c = Serial.read();
            
            if (c == '\n') {
                rxBuffer[rxIndex] = '\0'; // Завершаем C-строку
                
                // ============================================
                // КОМАНДА 'R': Одиночное чтение: "R <mux> <ch> <pwr>"
                // ============================================
                if (rxBuffer[0] == 'R') {
                    int mux, ch, pwr;
                    if (sscanf(rxBuffer, "R %d %d %d", &mux, &ch, &pwr) == 3) {
                        int id = (mux * 8) + ch;
                        
                        if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE) {
                            scanAndReportAntenna(id, pwr);
                            xSemaphoreGive(uartMutex);
                        }
                    }
                } 
                // ============================================
                // КОМАНДА 'S': Чтение всей доски: "S <pwr>"
                // ============================================
                else if (rxBuffer[0] == 'S') {
                    int pwr;
                    if (sscanf(rxBuffer, "S %d", &pwr) == 1) {
                        if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE) {
                            Serial.println("SCAN_START");
                            
                            for (int id = 0; id < 96; id++) {
                                scanAndReportAntenna(id, pwr);
                                
                                // Каждые 8 антенн отдаем ядро планировщику (1 мс)
                                // Это предотвращает срабатывание Task Watchdog (TWDT)
                                if (id % 8 == 0) {
                                    vTaskDelay(pdMS_TO_TICKS(1));
                                }
                            }
                            Serial.println("SCAN_END");
                            xSemaphoreGive(uartMutex);
                        }
                    }
                }
                // ============================================
                // КОМАНДА 'I': Экстренный сброс чипа MFRC522 (Self-Healing)
                // ============================================
                else if (rxBuffer[0] == 'I') {
                    if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE) {
                        rfidReader.softReset();
                        Serial.println("OK INIT");
                        xSemaphoreGive(uartMutex);
                    }
                }
                
                // Сбрасываем буфер для следующей команды
                rxIndex = 0; 
                
            } else if (c != '\r' && rxIndex < sizeof(rxBuffer) - 1) {
                rxBuffer[rxIndex++] = c; // Накапливаем символы (с защитой от переполнения)
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    Serial.begin(115200);
    // Для ESP32-S3 ожидание Serial нужно, если используется встроенный USB-CDC
    while(!Serial && millis() < 3000); 

    SPI.begin(Config::SCK, Config::MISO, Config::MOSI, Config::SS);
    
    matrix.init();
    
    if (!rfidReader.init()) {
        Serial.println("SYS:ERROR: RFID INIT FAILED");
    } else {
        rfidReader.setPower(7); 
        Serial.println("SYS:OK: System ready");
    }

    uartMutex = xSemaphoreCreateMutex();
    
    // Создаем задачу с приоритетом 1. Памяти в 4096 байт хватит для парсинга и логики
    xTaskCreate(TaskUART, "UART_Command", 4096, nullptr, 1, nullptr);
}

void loop() {
    // Удаляем системную задачу loop. Экономим около 8КБ оперативной памяти.
    vTaskDelete(nullptr); 
}