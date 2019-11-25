#include <TM1637Display.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Module connection pins (Digital Pins)
#define CLK 4
#define DIO 3
#define ONE_WIRE_BUS 2

const uint8_t SEG_ERR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_E | SEG_G,                         // r
  SEG_E | SEG_G,                         // r
  0,                                     // space
};

TM1637Display display(CLK, DIO);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensor;
float temp;

int PIN_ALARM = 11;
bool IN_ALARM_MODE = false; // Состояние Аварийного режима (контролируется программой): true - вкл, false - выкл
int CRITICAL_TEMP = 10; // Критическая температура, когда срабатывает Аварийка
int NORMAL_TEMP = 20; // Нормальная температура, когда отключается Аварийка после срабатывания

void setup() {
    pinMode(PIN_ALARM, OUTPUT);
    display.setBrightness(0x02);
    sensors.begin();
    if (sensors.getDS18Count() == 0) display.setSegments(SEG_ERR);
    else sensors.setResolution(12);
    sensors.getAddress(sensor, 0); // Получаем адрес темп сенсора под номером 0 и сохраняем его в переменную sensor
}

void loop() {
    if (sensors.getDS18Count() != 0) {
        sensors.requestTemperatures();
        temp = ds.getTempC(sensor);
        display.showNumberDecEx(int(temp), 0b00000000, false);

        if (temp <= CRITICAL_TEMP) {
            if (IN_ALARM_MODE == false) {
                IN_ALARM_MODE = true;
                digitalWrite(PIN_ALARM, HIGH);
            }
        }
        else if (temp >= NORMAL_TEMP) {
            if (IN_ALARM_MODE == true) {
                IN_ALARM_MODE = false;
                digitalWrite(PIN_ALARM, LOW);
            }
        }
    }
    delay(1000);
}
