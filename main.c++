#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTPIN 12 
#define DHTTYPE DHT22

LiquidCrystal_I2C lcd(0x27, 20, 4);

BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

const int pinBuzzer = 10;
const int pinRainSensor = A0;
const int pinPresenceSensor = 3;

int valorSensor = 0;
int porcentagemChuva = 0;

void setup() {
  Wire.begin(); 
  lcd.init();
  lcd.backlight();

  Serial.begin(115200);

  dht.begin();

  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinPresenceSensor, INPUT);

  Serial.println("Iniciando sensores...");

  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  delay(2000);
  lcd.clear();

  

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 iniciado com sucesso.");
  } else {
    Serial.println("Erro ao iniciar BH1750.");
    lcd.print("Erro BH1750");
    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("Sistema iniciado!");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Sensor de chuva
  valorSensor = analogRead(pinRainSensor);
  porcentagemChuva = map(valorSensor, 1023, 0, 0, 100);

  // Sensor de presença
  byte estadoPresenca = digitalRead(pinPresenceSensor);
  if (estadoPresenca == HIGH) {
    Serial.println("Movimento detectado!");
    tone(pinBuzzer, 1000, 200);
  } else {
    Serial.println("Sem movimento.");
  }

  // Sensor de luminosidade
  float lux = lightMeter.readLightLevel();
  Serial.print("Luminosidade: ");
  Serial.print(lux);
  Serial.println(" lux");

  // LCD - Linha 1: Chuva
  lcd.setCursor(0, 0);
  lcd.print("Chuva: ");
  lcd.print(porcentagemChuva);
  lcd.print("%     ");

  Serial.print("Chuva: ");
  Serial.print(porcentagemChuva);
  Serial.println("%");

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Falha ao ler o DHT22!");
  }
  
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.println(" °C");
  Serial.print("Umi: ");
  Serial.print(hum);
  Serial.println(" %");

  // LCD - Linha 2: Luminosidade
  lcd.setCursor(0, 1);
  if (lux >= 500){
    lcd.print("Luz: Acesa");
  }
  else
  {
    lcd.print("Luz: Apagada");
  }
  

  delay(1000);
}