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

#define LIMIAR_LUZ 40

// Flags e variáveis de controle
volatile bool flagChuva = false;
volatile bool flagAtualizarLEDs = false;
volatile bool flagAtualizarDHT = false;
volatile bool flagPrintChuva = false;

volatile bool estaChovendo = false;

volatile float ultimaTemperatura = 0;
volatile float ultimaUmidade = 0;

// Controle do buzzer via Timer1
volatile bool buzzerLigado = false;
volatile uint16_t contadorBuzzer = 0;
const uint16_t DURACAO_BUZZER_MS = 1000; // 1 segundo

// Debounce do sensor de chuva
volatile bool flagEventoChuva = false;
volatile uint16_t contadorDebounceChuva = 0;
const uint16_t TEMPO_DEBOUNCE_CHUVA = 200;
volatile bool estadoChuvaTentativa = false;

void verificarLuminosidade();
void atualizarDisplay();

void setup() {
  Wire.begin();
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  dht.begin();
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  // LEDs e buzzer como saída
  DDRB |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << BUZZER_PIN);

  // Configura pinos de entrada com pull-up
  DDRD &= ~((1 << PD2) | (1 << PD3));   // D2 (presença), D3 (chuva)
  PORTD |= (1 << PD2) | (1 << PD3);     // pull-up

  // INT0 – sensor de presença (D2), borda de subida
  EICRA |= (1 << ISC01) | (1 << ISC00);
  EIMSK |= (1 << INT0);

  // INT1 – sensor de chuva (D3), qualquer mudança de nível
  EICRA |= (1 << ISC10);     // ISC11 = 0, ISC10 = 1 → qualquer mudança
  EIMSK |= (1 << INT1);

  // Timer1: modo CTC, prescaler 8, interrupção a cada 1ms
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  OCR1A = 1999;  // 1ms com clock 16 MHz e prescaler 8
  TIMSK1 = (1 << OCIE1A);

  sei(); // habilita interrupções globais

  lcd.setCursor(0, 0);
  lcd.print("Sistema Iniciado");
  delay(2000);
  lcd.clear();

  while (1) {
    if (flagChuva) {
      flagChuva = false;
      atualizarDisplay();
    }

    if (flagPrintChuva) {
      flagPrintChuva = false;
      Serial.print("Sensor chuva: ");
      Serial.println(estaChovendo ? "CHOVENDO" : "SEM CHUVA");
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
  }
}

// Timer1 a cada 1ms
ISR(TIMER1_COMPA_vect) {
  static uint16_t contador1ms = 0;
  static uint16_t contadorLuz = 0;

  contador1ms++;
  contadorLuz++;

  if (contador1ms >= 2000) {  // Atualiza DHT a cada 2s
    flagAtualizarDHT = true;
    contador1ms = 0;
  }

  if (contadorLuz >= 1000) {  // Atualiza luminosidade a cada 1s
    flagAtualizarLEDs = true;
    contadorLuz = 0;
  }

  // Controle buzzer
  if (buzzerLigado) {
    contadorBuzzer++;
    if (contadorBuzzer >= DURACAO_BUZZER_MS) {
      buzzerLigado = false;
      PORTB &= ~(1 << BUZZER_PIN);
    }
  }

  // Debounce da chuva
  if (flagEventoChuva) {
    contadorDebounceChuva++;
    if (contadorDebounceChuva >= TEMPO_DEBOUNCE_CHUVA) {
      bool novoEstado = !(PIND & (1 << PD3)); // ativo baixo
      if (estaChovendo != novoEstado) {
        estaChovendo = novoEstado;
        flagChuva = true;
        flagPrintChuva = true;
      }
      flagEventoChuva = false;
    }
  }
}

// Sensor de presença (INT0 - D2)
ISR(INT0_vect) {
  if (PIND & (1 << PD2)) {
    buzzerLigado = true;
    contadorBuzzer = 0;
    PORTB |= (1 << BUZZER_PIN);
  }
}

// Sensor de chuva (INT1 - D3)
ISR(INT1_vect) {
  estadoChuvaTentativa = !(PIND & (1 << PD3));  // ativo baixo
  flagEventoChuva = true;
  contadorDebounceChuva = 0;
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
