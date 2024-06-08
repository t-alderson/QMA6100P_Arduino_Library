// SparkFun_Qwiic_QMA6100P.h
//
// This is a library written for the SparkFun Triple Axis Accelerometer - QMA6100P


// The following class implements the	methods to set, get, and read from the SparkFun Triple
// Axis Acceleromter - QMA6100P.

#pragma once

#include <Wire.h>
#include "QMA6100P_regs.h"
#include "sfe_bus.h"

#define QMA6100P_ADDRESS_HIGH 0x13
#define QMA6100P_ADDRESS_LOW 0x12

#define QMA6100P_CHIP_ID 0x90

// CNTL1 GSEL<1:0>
#define SFE_QMA6100P_RANGE2G 0b0001
#define SFE_QMA6100P_RANGE4G 0b0010
#define SFE_QMA6100P_RANGE8G 0b0100
#define SFE_QMA6100P_RANGE16G 0b1000
#define SFE_QMA6100P_RANGE32G 0b1111

#define TOTAL_ACCEL_DATA_8BIT 3
#define TOTAL_ACCEL_DATA_16BIT 6

#define XLSB 0
#define XMSB 1
#define YLSB 2
#define YMSB 3
#define ZLSB 4
#define ZMSB 5

#define DEFAULT_SETTINGS 0xC0 // CNTL1: Hi-Performance mode; Data-Ready disabled; minimum G-range; Tap & Tilt disabled
#define INT_SETTINGS 0xE0 // CNTL1: Hi-Performance mode; Data-Ready enabled; minimum G-range; Tap & Tilt disabled
#define SOFT_INT_SETTINGS 0xE1
#define BUFFER_SETTINGS 0xE2
#define TILT_SETTINGS 0xE3

struct outputData
{
  float xData;
  float yData;
  float zData;
};

struct rawOutputData
{
  int16_t xData;
  int16_t yData;
  int16_t zData;
};

class QwDevQMA6100P
{
public:
  QwDevQMA6100P() : _i2cAddress{0}, _cs{0} {};

  int writeRegisterRegion(uint8_t reg, uint8_t *data, uint16_t length);
  int writeRegisterByte(uint8_t reg, uint8_t data);
  int readRegisterRegion(uint8_t reg, uint8_t *data, uint16_t length);
  void setCommunicationBus(sfe_QMA6100P::QwIDeviceBus &theBus, uint8_t i2cAddress);

  uint8_t getUniqueID();
  bool initialize(uint8_t settings = DEFAULT_SETTINGS);

  bool init(void);
  bool getAccelData(outputData *userData);
  bool convAccelData(outputData *userAccel, rawOutputData *rawAccelData);

  // General Settings
  bool enableAccel(bool enable = true);
  bool softwareReset();
  int8_t getOperatingMode();
  bool setRange(uint8_t);
  bool setInterruptPin(bool enable, uint8_t polarity = 0, uint8_t pulseWidth = 0, bool latchControl = false);
  bool enableDataEngine(bool enable = true);
  bool setOutputDataRate(uint8_t);
  float getOutputDataRate();
  bool dataReady();
  bool runCommandTest();
  uint8_t readAccelState();
  bool getRawAccelData(rawOutputData *);
  bool getRawAccelRegisterData(rawOutputData *);
  bool getRawAccelBufferData(rawOutputData *, int sixteenBit = -1); // Set sixteenBit to 0 to read 8-bit data. Set to 1 to read 16-bit data.
  float readOutputDataRate();

  // Tap/Double settings
  bool enableTapEngine(bool enable = true);
  bool setTapDataRate(uint8_t rate);
  bool enableDirecTapInterupt(bool enable = true);
  bool enableDoubleTapInterrupt(bool enable = true);

  // Tilt Settings
  bool enableTiltEngine(bool enable = true);
  bool setTiltDataRate(uint8_t rate);

  // Wake/Sleep Settings
  bool setWakeDataRate(uint8_t rate);
  bool enableSleepEngine(bool enable = true);
  bool enableWakeEngine(bool enable = true);
  bool forceWake();
  bool forceSleep();

  // Interrupt Settings
  bool configureInterruptPin(uint8_t pinVal);
  bool routeHardwareInterrupt(uint8_t, uint8_t pin = 1);
  bool enablePhysInterrupt(bool enable = true, uint8_t pin = 1);
  bool setPinMode(bool activeHigh = true, uint8_t pin = 1);
  bool setLatchControl(bool pulsed = true, uint8_t pin = 1);
  bool setPulseWidth(uint8_t width, uint8_t pin = 1);
  bool clearInterrupt();
  bool tapDetected();
  int8_t getDirection();
  bool unknownTap();
  bool waterMarkReached();
  bool bufferFull();
  bool freeFall();
  bool doubleTapDetected();
  bool tiltChange();

  // Buffer Setttings
  bool setBufferThreshold(uint8_t);
  bool setBufferOperationMode(uint8_t operationMode);
  bool setBufferResolution(bool sixteenBit = true);
  bool enableBufferInt(bool enable = true);
  bool enableSampleBuffer(bool enable = true);
  uint16_t getSampleLevel();
  bool clearBuffer();

  rawOutputData rawAccelData;

  // QMA6100P conversion values
  const double convRange2G = .000244;
  const double convRange4G = .000488;
  const double convRange8G = .000977;
  const double convRange16G = .001950;
  const double convRange32G = .003910;

protected:
  sfe_QMA6100P::QwIDeviceBus *_sfeBus;
  uint8_t _i2cAddress;
  uint8_t _cs;
  int _range = -1; // Keep a local copy of the range. Default to "unknown" (-1).
};