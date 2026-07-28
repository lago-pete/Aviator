#include <Arduino.h>
#include <Wire.h>
#include <vector>

struct Temp
{
  uint16_t par_t1;
  int16_t par_t2;
  int8_t par_t3;
} temp;

struct Pressure
{

  uint16_t par_p1;
  int16_t par_p2;
  int8_t par_p3;
  int16_t par_p4;
  int16_t par_p5;
  int8_t par_p6;
  int8_t par_p7;
  int16_t par_p8;
  int16_t par_p9;
  uint8_t par_p10;
} pressure;

struct Humidity
{
  uint16_t par_h1; // 0xE2 <3:0>/0xE3	 (12-bit, nibble-packed — special case)
  uint16_t par_h2; // 0xE2<7:4>/0xE1 (12-bit, nibble-packed — special case)
  int8_t par_h3;
  int8_t par_h4;
  int8_t par_h5;
  uint8_t par_h6;
  int8_t par_h7;
} humididty;

struct GasHeat
{
  int8_t par_g1;                // 0xED 		inside block 2
  int16_t par_g2;               // 0xEB/0xEC		inside block 2
  uint8_t par_g3;               // 0xEE		inside block 2
  int8_t res_heat_val;          //	0x00		standalone, own read
  int8_t range_switching_error; // 0x04		standalone, own read
  uint8_t res_heat_range;
} gasHeat;

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);
  Serial.println("Connecting....");

  uint8_t buffer1[25];
  uint8_t buffer2[16];
  // Code (binary)	Code (hex)	Meaning
  // 000	0x0	Skip this measurement (sensor disabled)
  // 001	0x1	x1 oversampling (1 sample, fastest, noisiest)
  // 010	0x2	x2 oversampling
  // 011	0x3	x4 oversampling
  // 100	0x4	x8 oversampling
  // 101	0x5	x16 oversampling (most samples, slowest, most accurate)
  // 110, 111	0x6, 0x7	Reserved/undefined — don't use
  uint8_t tempOS = 0x03; //x4
  uint8_t pressOS = 0x03; //x4
  uint8_t humOS = 0x01; //x1
  uint8_t mode = 0x01; // wake
  uint8_t meas = tempOS << 5 | pressOS << 2 | mode;

  // Ctrl_Hum
  Wire.beginTransmission(0x77);
  Wire.write(0x72);
  Wire.write(humOS); // This is the input for the Ctrl_hum with byte = unused | osrs_h 
  Wire.endTransmission();

  // Ctrl_meas
  Wire.beginTransmission(0x77);
  Wire.write(0x74);
  Wire.write(meas); // This is the input for the ctrl_meas with a byte = osrs_t | osrs_p | mode
  Wire.endTransmission();

  // Block 1
  Wire.beginTransmission(0x77);
  Wire.write(0x89);
  Wire.endTransmission(false);

  Wire.requestFrom(0x77, 25);
  for (int i = 0; i < 25; i++)
  {
    buffer1[i] = Wire.read();
    Serial.print("0x");
    Serial.print(buffer1[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
  Serial.print("-----------Block 1-------------");

  // Block 2
  Serial.println(" ");
  Serial.println("-----------Block 2-------------");
  Wire.beginTransmission(0x77);
  Wire.write(0xE1);
  Wire.endTransmission(false);

  Wire.requestFrom(0x77, 16);
  for (int i = 0; i < 16; i++)
  {
    buffer2[i] = Wire.read();
    Serial.print("0x");
    Serial.print(buffer2[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
  Serial.println("-----------Block 2-------------");

  // Extract Data
  // Temp
  temp.par_t1 = (buffer2[9] << 8 | buffer2[8]);
  uint16_t x = (uint16_t)(buffer1[2] << 8 | buffer1[1]);
  temp.par_t2 = (int16_t)x;
  temp.par_t3 = (int8_t)buffer1[3];

  // Pressure
  pressure.par_p1 = (buffer1[6] << 8 | buffer1[5]);
  uint16_t p2 = (uint16_t)(buffer1[8] << 8 | buffer1[7]);
  pressure.par_p2 = (int16_t)p2;
  pressure.par_p3 = (int8_t)buffer1[9];
  uint16_t p4 = (uint16_t)(buffer1[12] << 8 | buffer1[11]);
  pressure.par_p4 = (int16_t)p4;
  uint16_t p5 = (uint16_t)(buffer1[14] << 8 | buffer1[13]);
  pressure.par_p5 = (int16_t)p5;
  pressure.par_p7 = (int8_t)buffer1[15];
  pressure.par_p6 = (int8_t)buffer1[16];
  uint16_t p8 = (uint16_t)(buffer1[20] << 8 | buffer1[19]);
  pressure.par_p8 = (int16_t)p8;
  uint16_t p9 = (uint16_t)(buffer1[22] << 8 | buffer1[21]);
  pressure.par_p9 = (int16_t)p9;
  pressure.par_p10 = buffer1[23];

  // Humididty

  humididty.par_h3 = (int8_t)buffer2[3];
  humididty.par_h4 = (int8_t)buffer2[4];
  humididty.par_h5 = (int8_t)buffer2[5];
  humididty.par_h6 = buffer2[6];
  humididty.par_h7 = (int8_t)buffer2[7];
  humididty.par_h1 = (uint16_t)(buffer2[2] << 4) | (buffer2[1] & 0x0F);
  humididty.par_h2 = (uint16_t)(buffer2[0] << 4) | (buffer2[1] & 0xF0) >> 4;

  // Gas/Heat

  gasHeat.par_g1 = (int8_t)buffer2[12];
  uint16_t g2 = (uint16_t)(buffer2[11] << 8 | buffer2[10]);
  gasHeat.par_g2 = (int16_t)g2;
  gasHeat.par_g3 = (uint8_t)buffer2[13];

  // res_heat_val
  Wire.beginTransmission(0x77);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom(0x77, 1);
  gasHeat.res_heat_val = Wire.read();

  Wire.beginTransmission(0x77);
  Wire.write(0x04);
  Wire.endTransmission(false);
  Wire.requestFrom(0x77, 1);
  gasHeat.range_switching_error = Wire.read();

  Wire.beginTransmission(0x77);
  Wire.write(0x02);
  Wire.endTransmission(false);
  Wire.requestFrom(0x77, 1);
  uint8_t range = Wire.read();
  range = (range & 0x30) >> 4;
  gasHeat.res_heat_range = range;
}

void loop()
{
}

// // Block 1 — buffer1[], start 0x89

// Variable	Register	buffer1 index	Type
// par_t2	0x8A / 0x8B	1 / 2	int16_t
// par_t3	0x8C	3	int8_t
// par_p1	0x8E / 0x8F	5 / 6	uint16_t
// par_p2	0x90 / 0x91	7 / 8	int16_t
// par_p3	0x92	9	int8_t
// par_p4	0x94 / 0x95	11 / 12	int16_t
// par_p5	0x96 / 0x97	13 / 14	int16_t
// par_p7	0x98	15	int8_t
// par_p6	0x99	16	int8_t
// par_p8	0x9C / 0x9D	19 / 20	int16_t
// par_p9	0x9E / 0x9F	21 / 22	int16_t
// par_p10	0xA0	23	uint8_t

// Block 2 — buffer2[], start 0xE1

// Variable	Register	buffer2 index	Type
// par_h2 (MSB)	0xE1	0	uint16_t (special)
// par_h1/par_h2 packed	0xE2	1	special
// par_h1 (MSB)	0xE3	2	uint16_t (special)
// par_h3	0xE4	3	int8_t
// par_h4	0xE5	4	int8_t
// par_h5	0xE6	5	int8_t
// par_h6	0xE7	6	uint8_t
// par_h7	0xE8	7	int8_t
// par_t1	0xE9 / 0xEA	8 / 9	uint16_t
// par_g2	0xEB / 0xEC	10 / 11	int16_t
// par_g1	0xED	12	int8_t
// par_g3	0xEE	13	uint8_t