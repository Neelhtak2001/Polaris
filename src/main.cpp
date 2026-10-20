#include <Arduino.h>
#include <Wire.h>
#include <BleMouse.h>

BleMouse bleMouse("ESP32 Air Mouse", "Espressif", 100);

const int MPU_ADDR = 0x68;

const int SINAL_X = 1;
const int SINAL_Y = -1;

const float OFFSET_INICIAL_H = 0;
const float OFFSET_INICIAL_V = 0;

const int   TAM_JANELA     = 40;    // amostras por janela (40 x 12 ms = ~0,5 s)
const int   LIMITE_PARADO  = 60;    // variação máxima na janela p/ considerar "parado"
const int   OFFSET_MAX     = 3000;  // ignora janelas com média absurda
const float SUAVIZACAO     = 0.3;   // quão rápido o offset acompanha depois do 1º ajuste

float offH = OFFSET_INICIAL_H;
float offV = OFFSET_INICIAL_V;
bool convergiu = false;

int amostras = 0;
long somaH = 0, somaV = 0;
int16_t minH = 0, maxH = 0, minV = 0, maxV = 0;


bool lerGiro(int16_t &gHoriz, int16_t &gVert) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);

  if (Wire.requestFrom(MPU_ADDR, 6, true) != 6) {
    return false;
  }

  
  Wire.read();            // GyroX (alto), não usado
  Wire.read();            // GyroX (baixo), não usado
  int gyH = Wire.read();
  int gyL = Wire.read();
  int gzH = Wire.read();
  int gzL = Wire.read();

  int16_t gy = (int16_t)((gyH << 8) | gyL);
  int16_t gz = (int16_t)((gzH << 8) | gzL);

  gHoriz = gy;
  gVert  = gz;
  return true;
}

// praticamente parada, atualiza o offset automaticamnete
void atualizarOffset(int16_t h, int16_t v) {
  if (amostras == 0) {
    somaH = 0;
    somaV = 0;
    minH = maxH = h;
    minV = maxV = v;
  }

  somaH += h;
  somaV += v;
  if (h < minH) minH = h;
  if (h > maxH) maxH = h;
  if (v < minV) minV = v;
  if (v > maxV) maxV = v;
  amostras++;

  if (amostras < TAM_JANELA) {
    return;
  }
  amostras = 0;

  bool parado = ((maxH - minH) < LIMITE_PARADO) && ((maxV - minV) < LIMITE_PARADO);
  float mediaH = somaH / (float)TAM_JANELA;
  float mediaV = somaV / (float)TAM_JANELA;

  if (parado && fabs(mediaH) < OFFSET_MAX && fabs(mediaV) < OFFSET_MAX) {
    if (!convergiu) {
      offH = mediaH;
      offV = mediaV;
      convergiu = true;
      Serial.print("Offset inicial ajustado. H=");
      Serial.print(offH);
      Serial.print(" V=");
      Serial.println(offV);
    } else {
      offH += (mediaH - offH) * SUAVIZACAO;
      offV += (mediaV - offV) * SUAVIZACAO;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Serial.println("MPU6050 inicializado via I2C direto com sucesso!");

  bleMouse.begin();
  Serial.println("Bluetooth ativo! Procure por 'ESP32 Air Mouse' no computador.");
}

void loop() {
  int16_t gH, gV;

  // Lê sempre (conectado ou não), assim o offset já converge enquanto
  // o usuário ainda está pareando.
  if (lerGiro(gH, gV)) {
    atualizarOffset(gH, gV);

    if (bleMouse.isConnected()) {
      int hC = gH - (int)offH;
      int vC = gV - (int)offV;

      int moveX = SINAL_X * hC / 150;
      int moveY = SINAL_Y * vC / 180;

      if (abs(moveX) < 2) {
        moveX = 0;
      }

      if (abs(moveY) < 2) {
        moveY = 0;
      }

      moveX = constrain(moveX, -127, 127);
      moveY = constrain(moveY, -127, 127);

      if (moveX != 0 || moveY != 0) {
        bleMouse.move(moveX, moveY);
      }
    }
  }
  delay(12);
}