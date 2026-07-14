//SEm giroscopio
// Mapeamento de cores:
// Verde   -> Iniciar / ligar (andar frente)
// Amarelo -> Virar esquerda
// Azul    -> Virar direita
// Vermelho-> Parar
// Rosa    -> Comemorar
//
// LOG EEPROM:
// - Grava eventos enquanto anda solto (sem cabo)
// - Após parar, conecte o cabo e abra o Serial Monitor
// - O log é impresso automaticamente no setup()
// - Para apagar o log, recarregue o programa
// - ULTIMA_ATUALIZACAO_ => 02 de julho de 2026 - Mauricio

#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Servo.h> 
#include <DFRobot_BMI160.h>
#include <EEPROM.h>

// --- Configuração ---
#define PIN_SERVO_ESQ 2
#define PIN_SERVO_DIR 3

#define LedEnable_integrated 5
#define LedEnable_dedicated 6

// --- Calibração Servos ---
#define VELOCIDADE_L 40
#define VELOCIDADE_R 40
const int PARADO = 0;
const int velFrentEsq = VELOCIDADE_L;
const int velTrasEsq = -VELOCIDADE_L;    
const int VelFrentDir = -VELOCIDADE_R;
const int velTrasDir = VELOCIDADE_R;  

#define INTERVALO 15
#define TEMPO_CURVA_ESQ 1700 // <<< ajuste para curva esquerda (amarelo)
#define TEMPO_CURVA_DIR 1600 // <<< ajuste para curva direita  (azul)
#define LEITURAS_CHAO_NECESSARIAS 20

// --- EEPROM Log ---
// Cada entrada ocupa 4 bytes: [codigo_evento, R, G, B]
// Codigo de evento:
//   1 = VERDE detectado
//   2 = AMARELO detectado
//   3 = AZUL detectado
//   4 = VERMELHO detectado
//   5 = ROSA detectado
//   9 = leitura dentro de aguardarSaidaDaMarca (cor ainda vista)
#define EEPROM_START 0
#define EEPROM_MAX   200
#define BYTES_POR_ENTRADA 4

int eepromPos = EEPROM_START;

void logEEPROM(byte evento, byte r, byte g, byte b) {
  if (eepromPos + BYTES_POR_ENTRADA > EEPROM_START + EEPROM_MAX) return;
  EEPROM.write(eepromPos,     evento);
  EEPROM.write(eepromPos + 1, r);
  EEPROM.write(eepromPos + 2, g);
  EEPROM.write(eepromPos + 3, b);
  eepromPos += BYTES_POR_ENTRADA;
  if (eepromPos + BYTES_POR_ENTRADA <= EEPROM_START + EEPROM_MAX)
    EEPROM.write(eepromPos, 0xFF);
}

void imprimirLog() {
  Serial.println("===== LOG EEPROM =====");
  int pos = EEPROM_START;
  int entrada = 1;
  while (pos + BYTES_POR_ENTRADA <= EEPROM_START + EEPROM_MAX) {
    byte evento = EEPROM.read(pos);
    if (evento == 0xFF || evento == 0x00) break;
    byte r = EEPROM.read(pos + 1);
    byte g = EEPROM.read(pos + 2);
    byte b = EEPROM.read(pos + 3);
    Serial.print("#"); Serial.print(entrada++); Serial.print(" | ");
    switch (evento) {
      case 1: Serial.print("VERDE     "); break;
      case 2: Serial.print("AMARELO   "); break;
      case 3: Serial.print("AZUL      "); break;
      case 4: Serial.print("VERMELHO  "); break;
      case 5: Serial.print("ROSA      "); break;
      case 9: Serial.print("AGUARDANDO"); break;
      default: Serial.print("?         "); break;
    }
    Serial.print(" | R:"); Serial.print(r);
    Serial.print(" G:"); Serial.print(g);
    Serial.print(" B:"); Serial.println(b);
    pos += BYTES_POR_ENTRADA;
  }
  if (entrada == 1) Serial.println("(log vazio)");
  Serial.println("======================");
}

void limparEEPROM() {
  for (int i = EEPROM_START; i < EEPROM_START + EEPROM_MAX; i++)
    EEPROM.write(i, 0xFF);
  eepromPos = EEPROM_START;
}

// Flags de Estado
bool detectouCor = false; 
bool movimentoEnable = false;
int leiturasChao = 0;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_60X);
Servo servoEsq;
Servo servoDir;

int angular(int velocidade)
{
  return map(velocidade, -100, 100, 0, 180);
}

// --- Cores calibradas ---
int verde[3]    = {41,108,94};
int vermelho[3] = {130,62,61};
int azul[3]     = {39, 109, 164};
int amarelo[3]  = {119, 152, 51};
int rosa[3]     = {170,88, 125};

void parar() {
  servoEsq.write(angular(PARADO));
  servoDir.write(angular(PARADO));
}

void moverFrente() {
  servoEsq.write(angular(velFrentEsq));
  servoDir.write(angular(VelFrentDir));
}

void virarDireita() {
  servoEsq.write(angular(velFrentEsq));
  servoDir.write(angular(PARADO)); 
}

void virarEsquerda() {
  servoDir.write(angular(VelFrentDir));
  servoEsq.write(angular(PARADO));
}

void comemora() {
  servoDir.write(angular(VelFrentDir));
  servoEsq.write(angular(velTrasEsq));
}

bool verificaCor(float r, float g, float b, int alvo[]) {
  return (r > alvo[0] - INTERVALO && r < alvo[0] + INTERVALO) &&
         (g > alvo[1] - INTERVALO && g < alvo[1] + INTERVALO) &&
         (b > alvo[2] - INTERVALO && b < alvo[2] + INTERVALO);
}

void debugCor(float r, float g, float b) {
  Serial.print("R: "); Serial.print((int)r);
  Serial.print("  G: "); Serial.print((int)g);
  Serial.print("  B: "); Serial.print((int)b);
  Serial.print("   >> ");

  if      (verificaCor(r, g, b, verde))    Serial.println("VERDE");
  else if (verificaCor(r, g, b, vermelho)) Serial.println("VERMELHO");
  else if (verificaCor(r, g, b, azul))     Serial.println("AZUL");
  else if (verificaCor(r, g, b, amarelo))  Serial.println("AMARELO");
  else if (verificaCor(r, g, b, rosa))     Serial.println("ROSA");
  else                                     Serial.println("(chao/desconhecida)");
}

void aguardarSaidaDaMarca() {
  int contador = 0;
  while (contador < LEITURAS_CHAO_NECESSARIAS) {
    float r, g, b;
    tcs.getRGB(&r, &g, &b);
    bool algumaCor = verificaCor(r, g, b, verde)    ||
                     verificaCor(r, g, b, vermelho)  ||
                     verificaCor(r, g, b, azul)      ||
                     verificaCor(r, g, b, amarelo)   ||
                     verificaCor(r, g, b, rosa);
    if (!algumaCor) {
      contador++;
    } else {
      logEEPROM(9, (byte)r, (byte)g, (byte)b);
      contador = 0;
    }
    delay(20);
  }
  detectouCor = false;
  leiturasChao = 0;
}

void setup() 
{
  Serial.begin(9600);
  Serial.println("serial Initialized");

  imprimirLog();
  //limparEEPROM();

  servoEsq.attach(PIN_SERVO_ESQ);
  servoDir.attach(PIN_SERVO_DIR);
  parar(); 
  
  pinMode(LedEnable_integrated, OUTPUT);
  pinMode(LedEnable_dedicated, OUTPUT);
  
  analogWrite(LedEnable_integrated, 200); 
  analogWrite(LedEnable_dedicated, 90);

  if (tcs.begin()) 
  {
    Serial.println("Sensor RGB encontrado");
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
  tcs.getRGB(&r, &g, &b);
  debugCor(r, g, b);

  // Verde: iniciar
  if (verificaCor(r, g, b, verde)) 
  {
    if (!detectouCor && !movimentoEnable) 
    {
      detectouCor = true;
      movimentoEnable = true;
      logEEPROM(1, (byte)r, (byte)g, (byte)b);
      delay(3000);
      moverFrente();
      aguardarSaidaDaMarca();
    }
  }

  // Azul: virar direita
  else if (verificaCor(r, g, b, azul)) 
  {
    if (!detectouCor && movimentoEnable) 
    {
      detectouCor = true;
      logEEPROM(3, (byte)r, (byte)g, (byte)b);
      virarDireita();
      delay(TEMPO_CURVA_DIR);
      moverFrente();
      aguardarSaidaDaMarca();
    }
  }

  // Vermelho: parar
  else if (verificaCor(r, g, b, vermelho)) 
  {
    if (!detectouCor && movimentoEnable) 
    {
      detectouCor = true;
      logEEPROM(4, (byte)r, (byte)g, (byte)b);
      parar();
      movimentoEnable = false;
    }
  }

  // Amarelo: virar esquerda
  else if (verificaCor(r, g, b, amarelo)) 
  {
    if (!detectouCor && movimentoEnable) 
    {
      detectouCor = true;
      logEEPROM(2, (byte)r, (byte)g, (byte)b);
      virarEsquerda();
      delay(TEMPO_CURVA_ESQ);
      moverFrente();
      aguardarSaidaDaMarca();
    }
  }

  // Rosa: comemorar
  else if (verificaCor(r, g, b, rosa)) 
  {
    if (!detectouCor && movimentoEnable) 
    {
      detectouCor = true;
      logEEPROM(5, (byte)r, (byte)g, (byte)b);
      comemora();
      delay(5000);
      parar();
      movimentoEnable = false;
    }
  }

  else 
  {
    detectouCor = false;
  }
}
