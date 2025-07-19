#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <BH1750.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// LCD I2C (20 colunas, 4 linhas)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT22 conectado no pino D6
#define DHTPIN 6
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Sensor de luz I2C
BH1750 lightSensor;

// Buzzer e LEDs (PortB)
#define BUZZER_PIN PB2   // D10
#define LED1_PIN   PB0   // D8 (PB0)
#define LED2_PIN   PB1   // D9 (PB1)

// Limiar de luminosidade (lux)
#define LIMIAR_LUZ 50  // Ajuste conforme necessário

volatile bool flagPresenca = false;
volatile bool flagChuva = false;
volatile bool flagLuz = false;
volatile bool flagLuminosidade = false;

volatile bool estadoLed = false;
volatile uint16_t luminosidade = 0;

void acionarBuzzer(void);
void verificarLuminosidade();

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  dht.begin();
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  // Configura os pinos de saída CORRETAMENTE
  DDRB |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << BUZZER_PIN);

  // Configura os pinos de entrada com pull-up
  DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
  PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);

  // Configura interrupções
  EICRA |= (1 << ISC01) | (1 << ISC00);
  EIMSK |= (1 << INT0);
  EICRA |= (1 << ISC11) | (1 << ISC10);
  EIMSK |= (1 << INT1);
  PCICR |= (1 << PCIE2);      
  PCMSK2 |= (1 << PCINT20);

  // Timer1 para verificar luminosidade (1 segundo)
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
  OCR1A = 15624;
  TIMSK1 = (1 << OCIE1A);

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
    if(estadoLed) {
      PORTB |= (1 << LED1_PIN) | (1 << LED2_PIN);  
    } else {
      PORTB &= ~((1 << LED1_PIN) | (1 << LED2_PIN)); 
    }
  }

  if (flagLuminosidade) {
    flagLuminosidade = false;
    verificarLuminosidade();
  }

  
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    lastRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) {
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(t);
      lcd.print((char)223);
      lcd.print("C    ");
    }

    if (!isnan(h)) {
      lcd.setCursor(0, 1);
      lcd.print("Umi:  ");
      lcd.print(h);
      lcd.print("%    ");
    }
  }
}


ISR(INT0_vect) { flagPresenca = true; }
ISR(INT1_vect) { flagLuz = true; }
ISR(PCINT2_vect) { if (PIND & (1 << PD4)) flagChuva = true; }
ISR(TIMER1_COMPA_vect) { flagLuminosidade = true; }

void acionarBuzzer() {
  PORTB |= (1 << BUZZER_PIN);
  _delay_ms(200);
  PORTB &= ~(1 << BUZZER_PIN);
}

void verificarLuminosidade() {
  luminosidade = lightSensor.readLightLevel();
  
  
  if (luminosidade < LIMIAR_LUZ) {
    PORTB |= (1 << LED1_PIN) | (1 << LED2_PIN);  
  } else {
    PORTB &= ~((1 << LED1_PIN) | (1 << LED2_PIN)); 
  }
}