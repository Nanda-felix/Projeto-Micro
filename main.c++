#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// LCD I2C 
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT22 no pino D6
#define DHTPIN 6
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Buzzer e LED
#define BUZZER_PIN PB2   // D10
#define LED_PIN    PB0   // D8

// Flags
volatile bool flagPresenca = false;
volatile bool flagChuva = false;
volatile bool flagLuz = false;


volatile bool estadoLed = false;

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  dht.begin();

  
  DDRB |= (1 << BUZZER_PIN) | (1 << LED_PIN);

  
  DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
  PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);


  EICRA |= (1 << ISC01) | (1 << ISC00);
  EIMSK |= (1 << INT0);

 
  EICRA |= (1 << ISC11) | (1 << ISC10);
  EIMSK |= (1 << INT1);

 
  PCICR |= (1 << PCIE2);      
  PCMSK2 |= (1 << PCINT20);  

  sei(); 

  lcd.setCursor(0, 0);
  lcd.print("Sistema Iniciado");
  _delay_ms(2000);
  lcd.clear();
}

void loop() {
  if (flagPresenca) {
    flagPresenca = false;
    acionarBuzzer();
  }

  if (flagChuva) {
    flagChuva = false;
    lcd.setCursor(0, 0);
    lcd.print("Status: Chovendo ");
  }

  if (flagLuz) {
    flagLuz = false;
    estadoLed = !estadoLed;
    if (estadoLed) {
      PORTB |= (1 << LED_PIN);
    } else {
      PORTB &= ~(1 << LED_PIN);
    }
  }


  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    lastRead = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(t);
      lcd.print((char)223); 
      lcd.print("C    ");

      lcd.setCursor(0, 2);
      lcd.print("Umi:  ");
      lcd.print(h);
      lcd.print("%    ");
    }
  }
}


ISR(INT0_vect) {
  flagPresenca = true;
}


ISR(INT1_vect) {
  flagLuz = true;
}


ISR(PCINT2_vect) {

  if (PIND & (1 << PD4)) {
    flagChuva = true;
  }
}


void acionarBuzzer() {
  PORTB |= (1 << BUZZER_PIN);
  _delay_ms(200);
  PORTB &= ~(1 << BUZZER_PIN);
}
