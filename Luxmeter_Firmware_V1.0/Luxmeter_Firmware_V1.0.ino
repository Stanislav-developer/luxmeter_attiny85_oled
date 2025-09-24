/*
Luxmeter - Firmware V1.0

Author: Stanislav (GitHub: https://github.com/Stanislav-developer || Youtube: https://www.youtube.com/@TehnoMaisterna)
Repo:
Date: 01-06-2025
Ліцензія: MIT License – проєкт відкритий, вільно використовуй з зазначенням автора 

* Version: V1.0
  Функціонал:
  * Вимірювання освітленості від 0 до 54612 Lux
  * По натиску кнопки запам'ятовує попередню виміряну яскравість
  * Вимірює заряд акумулятору у відсотках(2.8V - 0%  4.2V - 100%)

* Конфігурація пінів:
  2 пін(PB3) - аналоговий вхід
  3 пін(PB4) - вхід з підтяжкою для кнопки
  5 пін(PB0) - SDA пін шини I2C для роботи з датчиком та дисплеєм
  7 пін(PB2) - SCL пін шини I2C для роботи з датчиком та дисплеєм

Налаштування загружчика:
cpu: attiny85
clock: internal 8 MHz

Скетч займає 7684 байти пам'яті мікроконтролера(93%)
*/

// Підключення бібліотек
#include <TinyWireM.h>
#include <Tiny4kOLED.h>
#include <EEPROM.h>

#define BH1750_ADDR 0x23  // Оголошуємо адресу(i2c) датчика BH1750
#define EEPROM_ADDR 0 // Оголошуємо адресу пам'яті eeprom

#define ADC_PIN A3 // Аналоговий пін для вимірювання напруги батареї
#define butPin 4      // пін кнопки
#define readDelay 30 // Затримка між семплами зчитування напруги батареї
#define delayTime2 450 // Час моргання індикації "LOW"
#define readSamples 5 // Кількість спроб виміряти напругу батареї
#define vcc 5.1 // Фактична напруга живлення мікроконтролеру від батареї або DC-DC перетворювача
#define debounceDelay 300 // час після якого детектиться нажаття кнопки

uint16_t lux = 0; // змінна у якій зберігається поточне значення люкс
uint16_t prevlux = 0; // змінна у якій зберігається попереднє значення люкс

int voltagePercent = 0; // змінна у якій зберігається рівень заряду акумулятора у відсотках

unsigned long prevTime1 = 0; // Змінна необхідна для коректної роботи функції readBattery
unsigned long prevTime2 = 0; // Змінна необхідна для коректної роботи функції chargeIndicate

bool state = false; // Змінна необхідна для коректної роботи функції chargeIndicate

unsigned long lastButtonTime = 0; // Змінна необхідна для коректної роботи функції buttonProcessing
bool lastButtonState = LOW; // Змінна необхідна для коректної роботи функції buttonProcessing
bool butFlag = false; // Змінна необхідна для коректної роботи функції buttonProcessing

// Бітмапа заставки(сайт для конвертування зображення у код:http://www.majer.ch/lcd/adf_bitmap.php)
const unsigned char Logo_Bitmap [] PROGMEM = {
0xFC, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00,
0x00, 0x80, 0x80, 0xE0, 0x30, 0x18, 0x08, 0x0C, 0x04, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x1C, 0x70, 0xC0, 0x70, 0x1C, 0x00, 0x00, 0x08, 0xFC, 0x00,
0x80, 0x00, 0x78, 0xCC, 0xCC, 0x78, 0x00, 0x00, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00, 0x96, 0x93, 0x91,
0xD8, 0x48, 0x4C, 0x64, 0x26, 0x22, 0x33, 0x11, 0x00, 0x00,
0x00, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0x69, 0xC9, 0x89, 0x1B, 0x12,
0x32, 0x26, 0x64, 0x44, 0xCC, 0x88, 0x00, 0x00, 0x00, 0xFF,
0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xFE, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x80,
0xFE, 0xFE, 0xFE, 0x00, 0x02, 0x06, 0x0E, 0x1E, 0xFC, 0xF8,
0xF0, 0xFC, 0xBE, 0x0E, 0x06, 0x02, 0x00, 0xFE, 0xFE, 0xFE,
0xFE, 0x0E, 0x0E, 0x06, 0x0E, 0xFE, 0xFE, 0xFC, 0xFC, 0x0E,
0x06, 0x06, 0x0E, 0xFE, 0xFE, 0xFC, 0x00, 0x00, 0x00, 0xF8,
0xFC, 0xFE, 0x6E, 0x66, 0x66, 0x66, 0x6E, 0x7E, 0x7C, 0x78,
0x00, 0x0E, 0x0E, 0xFF, 0xFF, 0xFF, 0x0E, 0x0E, 0x0E, 0x00,
0x00, 0xF8, 0xFC, 0xFE, 0x6E, 0x66, 0x66, 0x66, 0x6E, 0x7E,
0x7C, 0x78, 0x00, 0x00, 0xFE, 0xFE, 0xFE, 0xFE, 0x0E, 0x0E,
0x0E, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x07, 0x0C, 0x18,
0x10, 0x30, 0x20, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F,
0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x00, 0x00, 0x00,
0x07, 0x07, 0x0F, 0x0E, 0x0E, 0x0E, 0x0E, 0x07, 0x0F, 0x0F,
0x0F, 0x00, 0x08, 0x0C, 0x0E, 0x0F, 0x07, 0x01, 0x01, 0x03,
0x0F, 0x0F, 0x0C, 0x08, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x00,
0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00,
0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x01, 0x07, 0x07,
0x0F, 0x0E, 0x0C, 0x0C, 0x0E, 0x0F, 0x06, 0x04, 0x00, 0x00,
0x00, 0x07, 0x0F, 0x0F, 0x0E, 0x0C, 0x0E, 0x08, 0x00, 0x01,
0x07, 0x07, 0x0F, 0x0E, 0x0C, 0x0C, 0x0E, 0x0F, 0x06, 0x04,
0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00
};


// Ініціалізація BH1750
void bh1750_init() 
{
  TinyWireM.beginTransmission(BH1750_ADDR); //Починає передачу данних по шині I2C на адресу датчика BH1750
  TinyWireM.send(0x10); //Надсилає байт датчику щоб він почав надсилати данні вимірювать у режимі: H-Resolution Mode
  TinyWireM.endTransmission(); //Завершення ініціалізації, після цього датчик починає безперервно надсилати данні.
}

// Читання значення освітленості
uint16_t bh1750_read() 
{
  uint16_t value = 0; // створюємо змінну value (16-бітне число без знаку) для зберігання результату з датчика
  TinyWireM.requestFrom(BH1750_ADDR, 2); // запитуємо у датчика BH1750_ADDR 2 байти даних

  if (TinyWireM.available() == 2) //перевіряємо, чи дійсно отримано рівно 2 байти (щоб не брати “сміття”)
  {
    value  = TinyWireM.receive() << 8; //читаємо перший байт (старший), зсуваємо його вліво на 8 біт → він стане "старшими 8 бітами" результату
    value |= TinyWireM.receive(); //читаємо другий байт (молодший) і додаємо його до value. Тепер у змінній value зібране 16-бітне число від датчика.
  }
  return value / 1.2;  // перетворення у люкси
}

// Функція зчитування заряду акумулятора
float readBattery()
{
  int sum = 0; // Зберігає суму всіх семплів зчитання сирого аналогового сигналу
  int samplesTaken = 0; // Запам'ятовує скільки пройшло семплів зчитування заряду

  while (samplesTaken < readSamples) // Цикл у якому відбувається зчитування заряду 
  {
    if (millis() - prevTime1 >= readDelay) // Робимо затримку між вимірами
    {
      prevTime1 = millis();                 // оновлюємо час
      sum += analogRead(ADC_PIN);          // робимо вимір
      samplesTaken++;                      // рахуємо скільки вже виміряли
    }
  }
  int avgRead = (int)sum/readSamples; // Розраховуємо середнє значення вимірювання сирого аналогового сигналу

  float batLevel = (avgRead/1023.0)*vcc; // Перетворюємо аналогову інформацію у напругу

  float minV = 2.8;   // 0%
  float maxV = 4.2;   // 100%

  if (batLevel <= minV) return 0;
  if (batLevel >= maxV) return 100;

  // лінійне перетворення напруги у відсоток заряду від 0 до 100%
  return (int)((batLevel - minV) * 100.0 / (maxV - minV));
}

// Функція яка виводить на дисплей текст "LOW" після зменшення заряду 20%
void chargeIndicate() 
{
  //Реалізація моргання тексту на екрані

  if (state && millis() - prevTime2 >= delayTime2)
  {
    prevTime2 = millis(); 
    state = false;
    oled.setCursor(103, 2);
    oled.print(F(""));
    oled.clearToEOL(); 
  }

  else if (!state && millis() - prevTime2 >= delayTime2)
  {
    prevTime2 = millis(); // Попередній час = поточний час
    state = true;
    oled.setCursor(103, 2);
    oled.print(F("LOW!"));
    oled.clearToEOL();
  }
}

//Функція обробки натискання кнопки
void buttonProcessing()
{
  bool readButton = !digitalRead(butPin); //Зчитуємо стан кнопки та інвертуємо його оскільки є внутрішня підтяжка

  // перевірка на зміну стану та дебаунс
  if (readButton != lastButtonState) {
    lastButtonTime = millis(); // оновлюємо час зміни
  }

  if ((millis() - lastButtonTime) > debounceDelay) {
    // кнопка стабільна
    if (readButton == HIGH && !butFlag) 
    { // коротке натискання
      butFlag = true; 
      prevlux = lux; // Записуємо поточну яскравість у попередню
      EEPROM.put(EEPROM_ADDR, prevlux); // Зберігаємо це у EEPROM
    }
    if (readButton == LOW) 
    {
      butFlag = false; // кнопка відпущена
    }
  }

  lastButtonState = readButton;

}

void setup()
{
  TinyWireM.begin(); // запускаємо I2C

  bh1750_init(); // ініціалізуємо датчик

  pinMode(ADC_PIN, INPUT); // Пін для вимірювання заряду налаштовуємо як вхід
  pinMode(butPin, INPUT_PULLUP); // Пін кнопки налаштовуємо як вхід з підтяжкою до vcc

  EEPROM.get(EEPROM_ADDR, prevlux); // Отримуємо данні з еепром та записуємо у перемінну

  oled.begin(); // запускаємо OLED
  oled.clear(); // очищуємо дисплей
  oled.on(); // Вмикаємо дисплей

  oled.bitmap(0, 0, 128, 4, Logo_Bitmap); // Виводимо зображення бітмапи заставки на дисплей

  delay(1000); // Чекаємо 1 секунду
  oled.clear(); // Чистимо дисплей
}

void loop() 
{
  lux = bh1750_read(); // Зчитуємо значення з датчика

  buttonProcessing(); // Викликаємо функцію обробки кнопки

  oled.setFont(FONT8X16); // Встановлюємо стандартний шрифт для ОЛЕД дисплею

  // Виводимо на дисплей данні поточного рівня освітлення
  oled.setCursor(0, 16);
  oled.print(F("C:"));
  oled.print(lux);
  oled.print(F("Lx"));
  oled.clearToEOL(); 

  // Виводимо на дисплей данні попереднього збереженого рівня освітлення
  oled.setCursor(0, 2);
  oled.print(F("P:"));
  oled.print(prevlux);
  oled.print(F("Lx"));
  oled.print(F("    "));

  oled.setFont(FONT6X8); // Встановлюємо менший шрифт 

  voltagePercent = readBattery(); // Викликаємо функцію читання заряду батареї
  
  // Перевірка, якщо заряд низький
  if (voltagePercent <= 20)
  {
    chargeIndicate(); // Викликаємо функцію індикації низького рівня заряду
  }

  // Перевірка якщо заряд нормальний
  else if (voltagePercent >= 21)
  {
    // Просто малюємо напис BAT:
    oled.setCursor(103, 2);
    oled.print(F("BAT:"));
    oled.clearToEOL();
  }

  // Відображаємо процент заряду
  oled.setCursor(103, 3);
  oled.print(voltagePercent);
  oled.print(F("%"));
  oled.clearToEOL();

}
