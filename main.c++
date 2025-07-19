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
#define LIMIAR_LUZ 50

// Variáveis de controle
volatile bool flagPresenca = false;
volatile bool flagChuva = false;
volatile bool flagLuz = false;
volatile bool flagLuminosidade = false;
volatile bool flagAtualizarDHT = false;
volatile bool estaChovendo = false;

volatile bool estadoLed = false;
volatile uint16_t luminosidade = 0;
volatile float ultimaTemperatura = 0;
volatile float ultimaUmidade = 0;

void acionarBuzzer(void);
void verificarLuminosidade();
void atualizarDisplay();

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  dht.begin();
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  // Configura os pinos de saída
  DDRB |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << BUZZER_PIN);

  // Configura os pinos de entrada com pull-up
  DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
  PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);

  // Configura interrupções externas
  EICRA |= (1 << ISC01) | (1 << ISC00);  // INT0 borda de subida
  EIMSK |= (1 << INT0);
  EICRA |= (1 << ISC11) | (1 << ISC10);  // INT1 borda de subida
  EIMSK |= (1 << INT1);
  PCICR |= (1 << PCIE2);                 // Habilita PCINT16..23
  PCMSK2 |= (1 << PCINT20);              // Habilita PCINT20 (PD4)

  // Configura Timer1 para interrupções periódicas (1ms)
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC, prescaler 8
  OCR1A = 1999;  // 1ms @ 16MHz
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
    atualizarDisplay();
  }

  if (flagLuz) {
    flagLuz = false;
    estadoLed = !estadoLed;
    PORTB = (estadoLed) ? PORTB | (1 << LED1_PIN) | (1 << LED2_PIN) : 
                          PORTB & ~((1 << LED1_PIN) | (1 << LED2_PIN));
  }

  if (flagLuminosidade) {
    flagLuminosidade = false;
    verificarLuminosidade();
  }

  if (flagAtualizarDHT && !estaChovendo) {
    flagAtualizarDHT = false;
    
    // Faz a leitura do sensor
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t)) ultimaTemperatura = t;
    if (!isnan(h)) ultimaUmidade = h;
    
    atualizarDisplay();
  }
}

// Interrupção do Timer1 (1ms)
ISR(TIMER1_COMPA_vect) {
  static uint16_t contador1ms = 0;
  contador1ms++;
  
  // Verificação de luminosidade a cada 1000ms (1s)
  if(contador1ms % 1000 == 0) {
    flagLuminosidade = true;
  }
  
  // Leitura DHT e atualização a cada 2000ms (2s)
  if(contador1ms >= 2000) {
    flagAtualizarDHT = true;
    contador1ms = 0;
  }
}

// Outras ISRs permanecem iguais
ISR(INT0_vect) { flagPresenca = true; }
ISR(INT1_vect) { flagLuz = true; }
ISR(PCINT2_vect) { 
  static unsigned long lastRainTime = 0;
  if (millis() - lastRainTime > 200) {
    estaChovendo = !(PIND & (1 << PD4)); // Lógica corrigida
    flagChuva = true;
    lastRainTime = millis();
  }
}

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
    lcd.print("%    ");
  }
}