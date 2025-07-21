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

// LED1 e LED2 (PortB)
#define LED1_PIN PB0  // D8
#define LED2_PIN PB1  // D9

#define BUZZER_PIN 10 // D10 – buzzer passivo
#define LIMIAR_LUZ 50

volatile bool flagChuva = false;
volatile bool flagLuz = false;
volatile bool flagLuminosidade = false;
volatile bool flagAtualizarDHT = false;
volatile bool estaChovendo = false;
volatile bool flagPresencaDetectada = false;

volatile float ultimaTemperatura = 0;
volatile float ultimaUmidade = 0;

void verificarLuminosidade();
void atualizarDisplay();

void setup() {
  Wire.begin();
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  dht.begin();
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Configura LEDs como saída
  DDRB |= (1 << LED1_PIN) | (1 << LED2_PIN);

  // Configura pinos de entrada com pull-up (PD2 = INT0, PD3 = INT1, PD4 = sensor de chuva)
  DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
  PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);

  // Interrupções externas
  // INT0 – borda de subida (PD2 – presença)
  EICRA |= (1 << ISC01) | (1 << ISC00);
  EIMSK |= (1 << INT0);

  // INT1 – alternar luz (PD3)
  EICRA |= (1 << ISC11) | (1 << ISC10);
  EIMSK |= (1 << INT1);

  // PCINT2 – sensor de chuva (PD4)
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20);

  // Timer1 – CTC, prescaler 8, 1ms por interrupção
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  OCR1A = 1999;
  TIMSK1 = (1 << OCIE1A);

  sei(); // Habilita interrupções globais

  lcd.setCursor(0, 0);
  lcd.print("Sistema Iniciado");
  delay(2000);
  lcd.clear();
}

int main() {
  init(); // inicialização do Arduino core (Serial, timers, etc.)
  setup();

  while (1) {
    if (flagChuva) {
      flagChuva = false;
      atualizarDisplay();
    }

    if (flagLuz) {
      flagLuz = false;
      PORTB ^= (1 << LED1_PIN) | (1 << LED2_PIN);
    }

    if (flagLuminosidade) {
      flagLuminosidade = false;
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

    // Presença detectada – tocar buzzer com tom
    if (flagPresencaDetectada) {
      flagPresencaDetectada = false;
      Serial.println("Presença detectada!");

      tone(BUZZER_PIN, 1000, 1000);  // 2 kHz por 100 ms
    }
  }
}

// --- INTERRUPÇÕES ---

ISR(TIMER1_COMPA_vect) {
  static uint16_t contador1ms = 0;
  contador1ms++;

  if (contador1ms % 1000 == 0) flagLuminosidade = true;
  if (contador1ms >= 2000) {
    flagAtualizarDHT = true;
    contador1ms = 0;
  }
}

ISR(INT0_vect) {
  flagPresencaDetectada = true;
}

ISR(INT1_vect) {
  flagLuz = true;
}

ISR(PCINT2_vect) {
  static uint16_t debounce = 0;
  if (debounce++ > 200) {
    estaChovendo = !(PIND & (1 << PD4));
    flagChuva = true;
    debounce = 0;
  }
}

// --- FUNÇÕES AUXILIARES ---

void verificarLuminosidade() {
  uint16_t lux = lightSensor.readLightLevel();
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
