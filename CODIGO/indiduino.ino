//INDI-DUINO : 07/08/2026

//ligar os seguintes pinos:
//TCS3472 ----------- Arduino Nano
//VIn --------------- 5v
//Gnd --------------- Gnd
//Scl --------------- A5
//Sda --------------- A4
//Led --------------- d5 (Pwm)

//Servos tower 360 ----------- Arduino Nano
//in1 --------------- d2
//in2 --------------- d3

//Adicionar Biblioteca "Adafruit_TCS34725.h" ao Arduino IDE 2.0
//Possivel encontrar em https://github.com/adafruit/Adafruit_TCS34725
//adicionar via "bibliotecas" na arduino IDE 2.0 também é possivel, e BEM mais viável.


#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Servo.h> 

// --- Configuração ---
#define PIN_SERVO_ESQ 2
#define PIN_SERVO_DIR 3

#define LedEnable_integrated 5
#define LedEnable_dedicated 6

#define LED_INTEGRATED_BRIGHTNESS_BASE 96
#define LED_INTEGRATED_BRIGHTNESS_MIN 64
#define LED_INTEGRATED_BRIGHTNESS_MAX 127

#define LED_DEDICATED_BRIGHTNESS_BASE 143
#define LED_DEDICATED_BRIGHTNESS_MIN 140
#define LED_DEDICATED_BRIGHTNESS_MAX 178



// --- Calibração Servos ---
#define VELOCIDADE_L 50 // -------->Caso motores desiguais, calibrar aqui
#define VELOCIDADE_R 50

#define T_ESQ 1000 // -------> Tempo de curva para cada lado, calibrar aqui
#define T_DIR 1000

const int PARADO =0;
const int velFrentEsq = VELOCIDADE_L;
const int velTrasEsq = -VELOCIDADE_L;    
const int VelFrentDir = -VELOCIDADE_R;
const int velTrasDir = VELOCIDADE_R;  

#define INTERVALO 10


// Flags de Estado
bool detectouCor = false; 

bool velocidade = false;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_16X);
Servo servoEsq;
Servo servoDir;

// Mede a tensão de alimentação em mV usando o ADC interno do Arduino.
long readVcc()
{
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  long result = ADCL;
  result |= ADCH << 8;
  return 1125300L / result;
}


int angular(int velocidade)
{
  int dif_max = 100;
  int vccMv = readVcc();
  // Converte velocidade -100..100 para ângulo 0..180
  int anguloBase = map(velocidade, -100, 100, 0, 180);

  // Diferencial de compensação:
  // 5200 mV -> 0
  // 4500 mV -> DIF_MAX
  int diferencial = map(vccMv, 5000, 4000, 0, dif_max);

  // Limita o diferencial
  diferencial = constrain(diferencial, 0, dif_max);

  // Aplica a compensação de acordo com o sentido da velocidade
  if (velocidade > 0)
  {
      anguloBase += diferencial;
  }
  else if (velocidade < 0)
  {
      anguloBase -= diferencial;
  }

  // Garante que o servo/motor nunca receba algo fora de 0..180
  return constrain(anguloBase, 0, 180);
}

// Cores alvo (R, G, B)
int verde[3]    = {42, 105, 87};
int vermelho[3] = {132, 50, 58};
int azul[3]     = {29, 77, 129};
int amarelo[3]  = {90, 95, 48};
int rosa[3]     = {85, 52, 101};

void parar() 
{
  servoEsq.write(angular(PARADO));
  servoDir.write(angular(PARADO));
}

void moverFrente() 
{
  servoEsq.write(angular(velFrentEsq));
  servoDir.write(angular(VelFrentDir));
}

void virarDireita() 
{
  servoEsq.write(angular(velFrentEsq));
  servoDir.write(angular(PARADO)); 
}

void virarEsquerda() 
{
  servoDir.write(angular(VelFrentDir));
  servoEsq.write(angular(PARADO));
}

void comemora() 
{
  servoDir.write(angular(VelFrentDir));
  servoEsq.write(angular(velTrasEsq));
}

// Ajusta o brilho dos LEDs de acordo com a tensão medida. deve ser inversamente proporcional
void ajustarBrilhoLed(long vccMv)
{
  int brightnessIntegrated = map(vccMv, 5200, 4100, LED_INTEGRATED_BRIGHTNESS_BASE, LED_INTEGRATED_BRIGHTNESS_MAX);
  brightnessIntegrated = constrain(brightnessIntegrated, LED_INTEGRATED_BRIGHTNESS_MIN, LED_INTEGRATED_BRIGHTNESS_MAX);

  int brightnessDedicated = map(vccMv, 5200, 4100, LED_DEDICATED_BRIGHTNESS_BASE, LED_DEDICATED_BRIGHTNESS_MAX);
  brightnessDedicated = constrain(brightnessDedicated, LED_DEDICATED_BRIGHTNESS_MIN, LED_DEDICATED_BRIGHTNESS_MAX);

  analogWrite(LedEnable_integrated, brightnessIntegrated);
  analogWrite(LedEnable_dedicated, brightnessDedicated);
}
// veerifica se o RGB Alvo bate com o RGB lido pelo sensor, dentro de um intervalo de tolerância
bool verificaCor(float r, float g, float b, int alvo[]) 
{
  return (r > alvo[0] - INTERVALO && r < alvo[0] + INTERVALO) &&
         (g > alvo[1] - INTERVALO && g < alvo[1] + INTERVALO) &&
         (b > alvo[2] - INTERVALO && b < alvo[2] + INTERVALO);
}

void setup() 
{
  Serial.begin(9600);
  Serial.println("Comunicação Serial Inicializada");
  servoEsq.attach(PIN_SERVO_ESQ);
  servoDir.attach(PIN_SERVO_DIR);
  parar(); 
  
  pinMode(LedEnable_integrated, OUTPUT);
  pinMode(LedEnable_dedicated, OUTPUT);

  // Ajusta o brilho inicial dos LEDs com base na tensão medida.
  // os motores irão interferir, mas a leitura dinamica compensa isso.
  long vccInicial = readVcc();
  ajustarBrilhoLed(vccInicial);
  
  Serial.print("Vcc inicial: "); Serial.print(vccInicial); Serial.println(" mV");

  if (tcs.begin()) //não remover
  {
    Serial.println("Sensor RGB Inicializado");
  } 
  else 
  {
    while (1)
    {
      Serial.println("TCS34725 nao encontrado. reinicie o robô.");
    }
  }
}

void loop() 
{
  float r, g, b;

  tcs.getRGB(&r, &g, &b);//<------ leitura do sensor de cor

  // Mede a tensão e ajusta o brilho dos LEDs em tempo real.
  
  long vccMv = readVcc();//<------ leitura da tensão linha 5v do arduino

  // Debug (opcional, remova para performance)
  Serial.print("R: "); Serial.print(r); Serial.print(" G: "); Serial.print(g); Serial.print(" B: "); Serial.print(b); Serial.print(" Vcc: "); Serial.print(vccMv); Serial.println(" mV");
 

  ajustarBrilhoLed(vccMv);//ajuste dinamico de brilho.
  
  if (verificaCor(r, g, b, verde)) //<------ VERDE
  {
    if (!detectouCor && !velocidade) 
    {
      velocidade = true;
      Serial.println("Verde: FRENTE (tempo 3s)");
      delay(3000);
      moverFrente();
      detectouCor = true;
      
    }
  }
  else if (verificaCor(r, g, b, azul)) //<------ AZUL
  {
    if (!detectouCor && velocidade) 
    {
      Serial.println("Azul: VIRAR DIR + FRENTE");
      virarDireita();
      delay(T_DIR); 
      moverFrente();
      detectouCor = true;
    }
  }
  else if (verificaCor(r, g, b, vermelho)) //<------ VERMELHO
  {
    if (!detectouCor && velocidade) 
    {
      Serial.println("Vermelho: PARAR");
      parar();
      detectouCor = true;
      velocidade = false;//fim de percurso
    }
  }
  else if (verificaCor(r, g, b, rosa)) //<------ ROXO
  {
    if (!detectouCor && velocidade) 
    {
      Serial.println("Roxo: VIRAR ESQ + FRENTE");
      virarEsquerda();
      delay(T_ESQ); 
      moverFrente();
      detectouCor = true;
    }
  }
  else if (verificaCor(r, g, b, amarelo)) //<------ AMARELO
  {
    if (!detectouCor && velocidade) 
    {
      Serial.println("Amarelo: COMEMORA");
      comemora();
      delay(5000);
      parar();
      detectouCor = true;
      velocidade = false;//fim de percurso
    }
  }
  else // Nenhuma cor detectada <--- necessário para resetar a flag de detecção de cor
  {
    //Serial.println("Cor desconhecida / Chão");
    detectouCor = false; 
    
  }
}
