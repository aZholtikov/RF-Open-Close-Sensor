#include "Arduino.h"
#include "Arduino_FreeRTOS.h"
#include "RF24.h"
#include "avr/interrupt.h"
#include "semphr.h"
#include "EEPROM.h"
#include "ZHConfig.h"

void sendButtonStatus(void *pvParameters);
float getBatteryLevelCharge(void);
void loadConfig(void);
void saveConfig(void);

int16_t id{abs((int16_t)ID)};

RF24 radio(9, 10);
SemaphoreHandle_t buttonSemaphore;

void setup()
{
  EICRA |= (1 << ISC00);
  EIMSK |= (1 << INT0);
  ADCSRA &= ~(1 << ADEN);
  radio.begin();
  radio.setChannel(120);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setPayloadSize(14);
  radio.setAddressWidth(3);
  radio.setCRCLength(RF24_CRC_8);
  radio.setRetries(15, 15);
  radio.openWritingPipe(0xDDEEFF);
  radio.powerDown();
  buttonSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(sendButtonStatus, "Send Button Status", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

void loop()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  portENTER_CRITICAL();
  sleep_enable();
  portEXIT_CRITICAL();
  sleep_cpu();
  sleep_reset();
}

void sendButtonStatus(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    xSemaphoreTake(buttonSemaphore, portMAX_DELAY);
    rf_transmitted_data_t sensor{id, RFST_OPEN_CLOSE};
    sensor.value_1 = getBatteryLevelCharge() * 100;
    sensor.value_2 = digitalRead(PD2) ? OPEN : CLOSE; // Normally closed.
    // sensor.value_2 = digitalRead(PD2) ? CLOSE : OPEN; // Normally open.
    radio.powerUp();
    radio.flush_tx();
    radio.write(&sensor, sizeof(rf_transmitted_data_t));
    radio.powerDown();
    sei();
  }
  vTaskDelete(NULL);
}

float getBatteryLevelCharge()
{
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
  ADCSRA |= (1 << ADEN);
  delay(10);
  ADCSRA |= (1 << ADSC);
  while (bit_is_set(ADCSRA, ADSC))
    ;
  ADCSRA &= ~(1 << ADEN);
  float value = ((1024 * 1.1) / (ADCL + ADCH * 256));
  return value;
}

void loadConfig()
{
  cli();
  if (EEPROM.read(511) == 254)
    EEPROM.get(0, id);
  else
    saveConfig();
  delay(50);
  sei();
}

void saveConfig()
{
  cli();
  EEPROM.write(511, 254);
  EEPROM.put(0, id);
  delay(50);
  sei();
}

ISR(INT0_vect)
{
  cli();
  xSemaphoreGiveFromISR(buttonSemaphore, NULL);
}