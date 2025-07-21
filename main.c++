#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <BH1750.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// LCD I2C (20 colunas, 4 linhas)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT22 no pino D6
#define DHTPIN 6
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Sensor de luz I2C
BH1750 lightSensor;

// Buzzer e LEDs (PortB)
#define BUZZER_PIN PB2  // D10
#define LED1_PIN PB0    // D8
#define LED2_PIN PB1    // D9

#define LIMIAR_LUZ 50

// Flags e variáveis de controle
volatile bool flagChuva = false;
volatile bool flagLuminosidade = false;    // **removida a atribuição do timer para esta flag no código**
volatile bool flagAtualizarDHT = false;
volatile bool flagAtualizarLEDs = false;   // NOVA flag para INT1

volatile bool estaChovendo = false;

volatile float ultimaTemperatura = 0;
volatile float ultimaUmidade = 0;

volatile bool presencaAtiva = false;

void verificarLuminosidade();
void atualizarDisplay();

void setup() {
  Wire.begin();
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  dht.begin();
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  // Configura pinos de saída
  DDRB |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << BUZZER_PIN);

  // Configura pinos de entrada com pull-up
  DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
  PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);

  // Interrupções externas INT0 para sensor de presença em qualquer borda
  EICRA |= (1 << ISC00);   // qualquer mudança
  EICRA &= ~(1 << ISC01);
  EIMSK |= (1 << INT0);

  // INT1 – borda de subida para atualizar LEDs
  EICRA |= (1 << ISC11) | (1 << ISC10);
  EIMSK |= (1 << INT1);

  // PCINT para PD4 (chuva)
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20);

  // Timer1: modo CTC, prescaler 8, interrupção a cada 1ms
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  OCR1A = 1999;  // 1ms com clock 16 MHz e prescaler 8
  TIMSK1 = (1 << OCIE1A);

  sei();

  lcd.setCursor(0, 0);
  lcd.print("Sistema Iniciado");
  delay(2000);
  lcd.clear();
}

void loop() {
  if (flagChuva) {
    flagChuva = false;
    atualizarDisplay();
  }

  if (flagAtualizarLEDs) {
    flagAtualizarLEDs = false;
    verificarLuminosidade();
  }

  if (flagAtualizarDHT && !estaChovendo) {
    flagAtualizarDHT = false;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) ultimaTemperatura = t;
    if (!isnan(h)) ultimaUmidade = h;

    atualizarDisplay();
  }

  // Liga/desliga buzzer conforme presença
  if (presencaAtiva) {
    PORTB |= (1 << BUZZER_PIN);
  } else {
    PORTB &= ~(1 << BUZZER_PIN);
  }
}

// Timer1 a cada 1ms
ISR(TIMER1_COMPA_vect) {
  static uint16_t contador1ms = 0;
  contador1ms++;

  // Atualiza DHT a cada 2s
  if (contador1ms >= 2000) {
    flagAtualizarDHT = true;
    contador1ms = 0;
  }
}

// ISR para sensor de presença (INT0) - detecta qualquer mudança
ISR(INT0_vect) {
  if (PIND & (1 << PD2)) {
    presencaAtiva = true;
    Serial.println("Presença ATIVA");
  } else {
    presencaAtiva = false;
    Serial.println("Presença INATIVA");
  }
}

// ISR INT1 - acionado para atualizar LEDs conforme sensor de luz
ISR(INT1_vect) {
  flagAtualizarLEDs = true;
}

// Chuva via PCINT2
ISR(PCINT2_vect) {
  static uint16_t debounce = 0;
  if (debounce++ > 200) {
    estaChovendo = !(PIND & (1 << PD4));
    flagChuva = true;
    debounce = 0;
  }
}

void verificarLuminosidade() {
  uint16_t lux = lightSensor.readLightLevel();
  Serial.print("Luminosidade: ");
  Serial.println(lux);

  if (lux < LIMIAR_LUZ) {
    PORTB |= (1 << LED1_PIN) | (1 << LED2_PIN);
  } else {
    PORTB &= ~((1 << LED1_PIN) | (1 << LED2_PIN));
  }
}

void atualizarDisplay() {
  lcd.clear();

  if (estaChovendo) {
    lcd.setCursor(0, 0);
    lcd.print("Status: Chovendo ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(ultimaTemperatura);
    lcd.print((char)223);
    lcd.print("C    ");

    lcd.setCursor(0, 1);
    lcd.print("Umi:  ");
    lcd.print(ultimaUmidade);
    lcd.print("%");
  }
}
