#include "Qwiic_QMA6100P.h"

uint8_t QwDevQMA6100P::getUniqueID()
{
  uint8_t tempVal;
  int retVal = readRegisterRegion(SFE_QMA6100P_CHIP_ID, &tempVal, 1);

  if (retVal != 0)
    return 0;

  return tempVal;
}

////////////////////////////////////////////////////////////////////////////////////
// setCommunicationBus()
//
// Method to set the bus object that is used to communicate with the device
//
//  Parameter:
//  theBus-The communication bus object
//  i2cAddress-I2C address for the 6DoF
void QwDevQMA6100P::setCommunicationBus(sfe_QMA6100P::QwIDeviceBus &theBus, uint8_t i2cAddress)
{
  _sfeBus = &theBus;
  _i2cAddress = i2cAddress;
}


// This function sets various register with regards to these pre-determined
// settings. These settings are set according to "AN092 Getting Started" guide and can easily
// have additional presets added.
bool QwDevQMA6100P::initialize(uint8_t settings)
{

  int retVal = 0;

  if (!enableAccel(true))
    return false;

  sfe_qma6100p_cntl1_bitfield_t cntl1;
  cntl1.all = 0; // Reset Value

  if (settings == DEFAULT_SETTINGS)
  {
    retVal = writeRegisterByte(SFE_QMA6100P_CNTL1, DEFAULT_SETTINGS);
    if (retVal == 0) // Check the write was successful
    {
      cntl1.all = DEFAULT_SETTINGS;
      _range = cntl1.bits.gsel; // Record the G-range
    }
  }

  if (settings == INT_SETTINGS)
  {
    enablePhysInterrupt();
    routeHardwareInterrupt(0x10);
    retVal = writeRegisterByte(SFE_QMA6100P_CNTL1, INT_SETTINGS);
    if (retVal == 0) // Check the write was successful
    {
      cntl1.all = INT_SETTINGS;
      _range = cntl1.bits.gsel; // Record the G-range
    }
  }

  if (settings == BUFFER_SETTINGS)
  {
    enablePhysInterrupt();
    routeHardwareInterrupt(0x40); // Buffer full interrupt
    enableSampleBuffer();         // Enable buffer
    setBufferOperationMode(0x00); // FIFO
    retVal = writeRegisterByte(SFE_QMA6100P_CNTL1, INT_SETTINGS);
    if (retVal == 0) // Check the write was successful
    {
      cntl1.all = INT_SETTINGS;
      _range = cntl1.bits.gsel; // Record the G-range
    }
  }

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// softwareReset()
//
// writing 0xB6 to 0x36, soft reset all of the registers. 
// After soft-reset, user should write 0x00 back
//

bool QwDevQMA6100P::softwareReset()
{
  if(writeRegisterByte(SFE_QMA6100P_SR, 0xb6) != 0)
    return false;

  sfe_qma6100p_sr_bitfeild_t sr;

  for(int i = 0; i < 10; i ++){
    if(readRegisterRegion(SFE_QMA6100P_SR, &sr.all, 1) != 0)
      return false;

    if(sr.all == 0xb6)
      break;
    delay(1);
  }

  if(writeRegisterByte(SFE_QMA6100P_SR, 0x00) != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableAccel()
//
// Enables accelerometer data. In addition
// some settings can only be set when the accelerometer is
// powered down
//
// Parameter:
// enable - enables or disables the accelerometer
//
//

bool QwDevQMA6100P::enableAccel(bool enable)
{

  uint8_t tempVal;

  if(readRegisterRegion(SFE_QMA6100P_PM, &tempVal, 1) != 0)
    return false;

  sfe_qma6100p_pm_bitfield_t pm;
  pm.all = tempVal;
  pm.bits.mode_bit = enable; // sets QMA6100P to active mode
  tempVal = pm.all;

  if(writeRegisterByte(SFE_QMA6100P_PM, tempVal) != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// getOperatingMode()
//
// Retrieves the current operating mode - low/high power mode
//

int8_t QwDevQMA6100P::getOperatingMode()
{

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL1, &tempVal, 1);

  if (retVal != 0)
    return retVal;

  sfe_qma6100p_cntl1_bitfield_t cntl1;
  cntl1.all = tempVal; // This is a long winded but definitive way of getting the operating mode bit
  _range = cntl1.bits.gsel; // Update the G-range

  return (cntl1.bits.pc1); // Return the operating mode bit
}

//////////////////////////////////////////////////
// setRange()
//
// Sets the operational g-range of the accelerometer.
//
// Parameter:
// range - sets the range of the accelerometer 2g - 32g depending
// on the version. 8g - 64g for the QMA6100P.
//
bool QwDevQMA6100P::setRange(uint8_t range)
{

  uint8_t tempVal;
  int retVal;

  if (range > SFE_QMA6100P_RANGE32G)
    return false;

  // Read - Modify - Write
  retVal = readRegisterRegion(SFE_QMA6100P_FSR, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_fsr_bitfield_t fsr;
  fsr.all = tempVal;
  fsr.bits.range =  range; // This is a long winded but definitive way of setting the range (g select)
  tempVal = fsr.all;

  retVal = writeRegisterByte(SFE_QMA6100P_FSR, tempVal);

  if (retVal != 0)
    return false;

  _range = range; // Update our local copy

  return true;
}

//////////////////////////////////////////////////
// enableDataEngine()
//
// Enables the data ready bit. and maps it to INT1
//
// Parameter:
// enable - enable/disables the data ready bit.
//
bool QwDevQMA6100P::enableDataEngine(bool enable)
{
  uint8_t tempVal;

  if(readRegisterRegion(SFE_QMA6100P_INT_MAP1, &tempVal, 1) != 0)
    return false;

  sfe_qma6100p_int_map1_bitfield_t int_map1;
  int_map1.all = tempVal;
  int_map1.bits.int1_data = enable; // data ready interrupt to INT1
  tempVal = int_map1.all;

  if(writeRegisterByte(SFE_QMA6100P_INT_MAP1, tempVal) != 0)
    return false;

  // enable data ready interrupt
  if(readRegisterRegion(SFE_QMA6100P_INT_EN1, &tempVal, 1) != 0)
    return false;

  sfe_qma6100p_int_en1_bitfield_t int_en1;
  int_en1.all = tempVal;
  int_en1.bits.int_data_en = enable; // set data ready interrupt
  tempVal = int_en1.all;

  if(writeRegisterByte(SFE_QMA6100P_INT_EN1, tempVal) != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableTapEngine()
//
// Enables the tap and double tap features of the accelerometers
//
// Parameter:
// enable - enables the tap/double tap feature
//
bool QwDevQMA6100P::enableTapEngine(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL1, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl1_bitfield_t cntl1;
  cntl1.all = tempVal;
  cntl1.bits.tdte =  enable; // This is a long winded but definitive way of setting/clearing the tap engine bit
  _range = cntl1.bits.gsel; // Update the G-range
  tempVal = cntl1.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL1, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableTiltEngine()
//
// Enables the tilt detection feature.
//
// Parameter:
// enable - enables the tilt feature
//
bool QwDevQMA6100P::enableTiltEngine(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL1, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl1_bitfield_t cntl1;
  cntl1.all = tempVal;
  cntl1.bits.tpe =  enable; // This is a long winded but definitive way of setting/clearing the tilt engine bit
  _range = cntl1.bits.gsel; // Update the G-range
  tempVal = cntl1.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL1, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableWakeEngine()
//
// Enables the wake detection feature.
//
// Parameter:
// enable - enables/disables the wake detection feature
//
bool QwDevQMA6100P::enableWakeEngine(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL4, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl4_bitfield_t cntl4;
  cntl4.all = tempVal;
  cntl4.bits.wufe =  enable; // This is a long winded but definitive way of setting/clearing the wake-up engine bit
  tempVal = cntl4.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL4, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableSleepEngine()
//
// Enables the sleep feature.
//
// Parameter:
// enable - enables/disables the sleep feature
//
bool QwDevQMA6100P::enableSleepEngine(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL4, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl4_bitfield_t cntl4;
  cntl4.all = tempVal;
  cntl4.bits.btse =  enable; // This is a long winded but definitive way of setting/clearing the back-to-sleep engine bit
  tempVal = cntl4.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL4, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setOutputDataRate()
//
// Changes the rate at which accelerometer data is generated.
//
// Parameter:
// rate - determines the rate to be applied.
//
bool QwDevQMA6100P::setOutputDataRate(uint8_t rate)
{

  if (rate > 15)
    return false;

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_ODCNTL, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_odcntl_bitfield_t odcntl;
  odcntl.all = tempVal;
  odcntl.bits.osa =  rate; // This is a long winded but definitive way of updating the ODR
  tempVal = odcntl.all;

  retVal = writeRegisterByte(SFE_QMA6100P_ODCNTL, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setTapDataRate()
//
// Changes the rate at which tap data is generated.
//
// Parameter:
// rate - determines the rate to be applied.
//
bool QwDevQMA6100P::setTapDataRate(uint8_t rate)
{

  if (rate > 7)
    return false;

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL3, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl3_bitfield_t cntl3;
  cntl3.all = tempVal;
  cntl3.bits.otdt = rate; // This is a long winded but definitive way of updating the tap ODR
  tempVal = cntl3.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL3, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setTiltDataRate()
//
// Changes the rate at which the tilt position is polled.
//
// Parameter:
// rate - determines the rate to be applied.
//
bool QwDevQMA6100P::setTiltDataRate(uint8_t rate)
{

  if (rate > 3)
    return false;

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL3, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl3_bitfield_t cntl3;
  cntl3.all = tempVal;
  cntl3.bits.otp = rate; // This is a long winded but definitive way of updating the tap ODR
  tempVal = cntl3.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL3, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setWakeDataRate()
//
// Changes the rate at which the wake function is performed.
//
// Parameter:
// rate - determines the rate to be applied.
//
bool QwDevQMA6100P::setWakeDataRate(uint8_t rate)
{

  if (rate > 7)
    return false;

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL3, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl3_bitfield_t cntl3;
  cntl3.all = tempVal;
  cntl3.bits.owuf = rate; // This is a long winded but definitive way of updating the wake-up ODR
  tempVal = cntl3.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL3, tempVal);

  if (retVal != 0)
    return false;

  return true;
}
//////////////////////////////////////////////////
// getOutputDataRate()
//
// Retrieves the output data rate of the accelerometer.
//
float QwDevQMA6100P::getOutputDataRate()
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_ODCNTL, &tempVal, 1);

  if (retVal != 0)
    return 0.0;

  sfe_qma6100p_odcntl_bitfield_t odcntl;
  odcntl.all = tempVal; // This is a long winded but definitive way of getting the ODR

  return (0.781 * (pow(2, (float)odcntl.bits.osa)));
}

//////////////////////////////////////////////////
// configureInterruptPin()
//
// This allows you to configure the entire interrupt register
//
// Parameter:
// pinVal - register value to set, note that this overwrites
// everything in the register.
//
bool QwDevQMA6100P::configureInterruptPin(uint8_t pinVal)
{

  int retVal;

  retVal = writeRegisterByte(SFE_QMA6100P_INC1, pinVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enablePhysInterrupt()
//
// Enables interrupts to be routed to the interrupt pins.
//
// Parameters:
// enable - Enables interrupts to report to the physical interrupt pins
// pin - This determines which pin to route the interrupts.
//
bool QwDevQMA6100P::enablePhysInterrupt(bool enable, uint8_t pin)
{
  int retVal = -1;
  uint8_t tempVal;

  if (pin > 2)
    return false;

  if (pin == 1)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC1, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc1_bitfield_t inc1;
    inc1.all = tempVal;
    inc1.bits.ien1 = enable; // This is a long winded but definitive way of setting/clearing the enable bit
    tempVal = inc1.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC1, tempVal);
  }

  if (pin == 2)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC5, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc5_bitfield_t inc5;
    inc5.all = tempVal;
    inc5.bits.ien2 = enable; // This is a long winded but definitive way of setting/clearing the enable bit
    tempVal = inc5.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC5, tempVal);
  }

  return (retVal == 0);
}

//////////////////////////////////////////////////
// setPinMode()
//
// Sets the active state of the physical interupt pins
//
// Parameters:
// enable - Enables interrupts to report to the physical interrupt pins
// pin - This determines which pin to route the interrupts.
//
bool QwDevQMA6100P::setPinMode(bool activeHigh, uint8_t pin)
{
  int retVal = -1;
  uint8_t tempVal;

  if (pin > 2)
    return false;

  if (pin == 1)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC1, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc1_bitfield_t inc1;
    inc1.all = tempVal;
    inc1.bits.iea1 = activeHigh; // This is a long winded but definitive way of setting/clearing the level bit
    tempVal = inc1.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC1, tempVal);
  }

  if (pin == 2)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC5, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc5_bitfield_t inc5;
    inc5.all = tempVal;
    inc5.bits.iea2 = activeHigh; // This is a long winded but definitive way of setting/clearing the level bit
    tempVal = inc5.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC5, tempVal);
  }

  return (retVal == 0);
}

//////////////////////////////////////////////////
// setLatchControl()
//
// Determines whether interrupts are pulsed (default) or latched.
// If they are latched then the interrupt must be released by reading
// the INT_REL register - clearInterrupt();
//
// Parameters:
// latch - False enables latch behavior, True enables pulse behavior (default)
//
bool QwDevQMA6100P::setLatchControl(bool pulsed, uint8_t pin)
{
  int retVal = -1;
  uint8_t tempVal;

  if (pin > 2)
    return false;

  if (pin == 1)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC1, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc1_bitfield_t inc1;
    inc1.all = tempVal;
    inc1.bits.iel1 = pulsed; // This is a long winded but definitive way of setting/clearing the latch bit
    tempVal = inc1.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC1, tempVal);
  }

  if (pin == 2)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC5, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc5_bitfield_t inc5;
    inc5.all = tempVal;
    inc5.bits.iel2 = pulsed; // This is a long winded but definitive way of setting/clearing the latch bit
    tempVal = inc5.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC5, tempVal);
  }

  return (retVal == 0);
}

//////////////////////////////////////////////////
// setPulseWidth()
//
// Determines the width of the interrupt pulse
//
// Parameters:
// width - The width setting to be applied.
// pin - the pin to be configured.
//
bool QwDevQMA6100P::setPulseWidth(uint8_t width, uint8_t pin)
{
  int retVal = -1;
  uint8_t tempVal;

  if ((width > 3) || (pin > 2))
    return false;

  if (pin == 1)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC1, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc1_bitfield_t inc1;
    inc1.all = tempVal;
    inc1.bits.pw1 = width; // This is a long winded but definitive way of setting the pulse width
    tempVal = inc1.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC1, tempVal);
  }

  if (pin == 2)
  {
    retVal = readRegisterRegion(SFE_QMA6100P_INC5, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_inc5_bitfield_t inc5;
    inc5.all = tempVal;
    inc5.bits.pw2 = width; // This is a long winded but definitive way of setting the pulse width
    tempVal = inc5.all;

    retVal = writeRegisterByte(SFE_QMA6100P_INC5, tempVal);
  }

  return (retVal == 0);
}

//////////////////////////////////////////////////
// routeHardwareInterrupt()
//
// This determines which interrupt is routed to a particular physical
// interrupt pin.
//
// Parameters:
// rdr - The selected interrupt - watermark, tap/double tap, tilt, data ready etc.
// pin - The physical hardware pin that will receive the interrupt.
//
bool QwDevQMA6100P::routeHardwareInterrupt(uint8_t rdr, uint8_t pin)
{

  int retVal;

  if (pin > 2)
    return false;

  if (pin == 1)
  {
    retVal = writeRegisterByte(SFE_QMA6100P_INC4, rdr);

    if (retVal != 0)
      return false;
  }

  if (pin == 2)
  {
    retVal = writeRegisterByte(SFE_QMA6100P_INC6, rdr);

    if (retVal != 0)
      return false;
  }

  return true;
}

//////////////////////////////////////////////////
// clearInterrupt()
//
// Clears any latched interrupt by reading the INT_REL register.
//
bool QwDevQMA6100P::clearInterrupt()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INT_REL, &tempVal, 1);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableDirecTapInterupt()
//
// Enables reporting on the direction of the latest generated tap.
//
// Parameter:
// enable - enables/disables directional tap reporting.
//
bool QwDevQMA6100P::enableDirecTapInterupt(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_TDTRC, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_tdtrc_bitfield_t tdtrc;
  tdtrc.all = tempVal;
  tdtrc.bits.stre = enable; // This is a long winded but definitive way of setting/clearing the enable bit
  tempVal = tdtrc.all;

  retVal = writeRegisterByte(SFE_QMA6100P_TDTRC, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableDirecTapInterupt()
//
// Enables the double tap interrupt.
//
// Parameter:
// enable - enables/disables the double tap interrupt
//
bool QwDevQMA6100P::enableDoubleTapInterrupt(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_TDTRC, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_tdtrc_bitfield_t tdtrc;
  tdtrc.all = tempVal;
  tdtrc.bits.dtre = enable; // This is a long winded but definitive way of setting/clearing the enable bit
  tempVal = tdtrc.all;

  retVal = writeRegisterByte(SFE_QMA6100P_TDTRC, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// dataReady()
//
// Checks the data ready bit indicating new accelerometer data
// is ready in the X/Y/Z Out regsiters. This is cleared automatically
// on read.
//
//
bool QwDevQMA6100P::dataReady()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return ins2.bits.drdy;
}

//////////////////////////////////////////////////
// freeFall()
//
// Checks the free fall interrupt bit indicating free fall
// has been detected.
//
bool QwDevQMA6100P::freeFall()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return ins2.bits.ffs;
}

//////////////////////////////////////////////////
// bufferFull()
//
// Checks the buffer full interrupt bit indicating that the
// buff is full.
//
bool QwDevQMA6100P::bufferFull()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return ins2.bits.bfi;
}

//////////////////////////////////////////////////
// waterMarkReached()
//
// Checks the watermark interrupt bit indicating it has been reached.
// buff is full.
//
bool QwDevQMA6100P::waterMarkReached()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return ins2.bits.wmi;
}

//////////////////////////////////////////////////
// tapDetected()
//
// Checks the tap interrupt bit indicating that a tap has
// been detected.
//
bool QwDevQMA6100P::tapDetected()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return (ins2.bits.tdts == 0x01); // Single tap
}

//////////////////////////////////////////////////
// getDirection()
//
// If the tap direction bit is enabled, this register will report
// the direction of the detected tap.
//
int8_t QwDevQMA6100P::getDirection()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS1, &tempVal, 1);

  if (retVal != 0)
    return retVal;

  return tempVal;
}

//////////////////////////////////////////////////
// unknowntap()
//
// if the accelerometer is unsure whether it has in fact
// detected a tap, it will report an "unknown" state. in that
// case this function will return true. good for error checking.
//
bool QwDevQMA6100P::unknownTap()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return (ins2.bits.tdts == 0x03); // undefined tap event
}

//////////////////////////////////////////////////
// doubleTapDetected()
//
// Checks the double tap interrupt bit indicating that
// a double tap has been detected.
//
bool QwDevQMA6100P::doubleTapDetected()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return (ins2.bits.tdts == 0x02); // Double tap
}

//////////////////////////////////////////////////
// tiltChange()
//
// Checks the tilt change interrupt bit indicating that
// the accelerometer has been tipped.
//
bool QwDevQMA6100P::tiltChange()
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_INS2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_ins2_bitfield_t ins2;
  ins2.all = tempVal;

  return (ins2.bits.tps); // Tilt position status
}

//////////////////////////////////////////////////
// setBufferThreshold()
//
// Sets the number of samples that can be held in the buffer.
//
// Parameter:
// threshold - This value determines the number of samples that
// will be store in the buffer. Can not exceed 171 for 8 bit resolution
// and 86 for 16 bit resolution.
//
bool QwDevQMA6100P::setBufferThreshold(uint8_t threshold)
{

  int retVal;
  uint8_t tempVal;

  if ((threshold < 2) || (threshold > 171))
    return false;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;

  // BRES – determines the resolution of the acceleration data samples collected by the sample buffer.
  // BRES = 0 – 8-bit samples are accumulated in the buffer
  // BRES = 1 – 16-bit samples are accumulated in the buffer

  if ((threshold > 86) && (bufCntl2.bits.bres == 1)) // 1 = 16bit resolution, max samples: 86
    threshold = 86;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CNTL1, threshold);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setBufferOperationMode()
//
// Sets the opertion mode of the Buffer: Bypass, FIFO, Stream, Trigger
//
// Parameter:
// operationMode - Determines the operation mode to set.
//
bool QwDevQMA6100P::setBufferOperationMode(uint8_t operationMode)
{

  int retVal;
  uint8_t tempVal;

  if (operationMode > 2)
    return false;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return true;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;
  bufCntl2.bits.bm = operationMode; // This is a long winded but definitive way of setting/clearing the operating mode
  tempVal = bufCntl2.all;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CNTL2, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// setBufferResolution()
//
// Sets the resoltuion of the data that is stored in the buffer: 8 or 16 bit.
//
// Parameter:
// sixteenBit - Determines whether the resolution is 16 (true) or 8 bit (false).
//
bool QwDevQMA6100P::setBufferResolution(bool sixteenBit)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;
  bufCntl2.bits.bres = sixteenBit; // This is a long winded but definitive way of setting/clearing the resolution bit
  tempVal = bufCntl2.all;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CNTL2, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableBufferInt()
//
// Enables the buffer full interrupt bit.
//
// Parameter:
// enable - enable/disables the buffer full interrupt bit.
//
bool QwDevQMA6100P::enableBufferInt(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;
  bufCntl2.bits.bfie = enable; // This is a long winded but definitive way of setting/clearing the buffer interrupt enable bit
  tempVal = bufCntl2.all;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CNTL2, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// enableSampleBuffer()
//
// Enables use of the buffer.
//
// Parameter:
// enable - enable/disables the buffer.
//
bool QwDevQMA6100P::enableSampleBuffer(bool enable)
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;
  bufCntl2.bits.bufe = enable; // This is a long winded but definitive way of setting/clearing the buffer enable bit
  tempVal = bufCntl2.all;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CNTL2, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// getSampleLevel()
//
// Gets the number of samples in the Buffer.
//
uint16_t QwDevQMA6100P::getSampleLevel()
{
  int retVal;
  uint8_t tempVal[2] = {0};
  uint16_t numSamples;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_STATUS_1, tempVal, 2);

  if (retVal != 0)
    return 0;

  numSamples = tempVal[0];
  numSamples |= (((uint16_t)tempVal[1] & 0x03) << 8);

  return numSamples;
}

//////////////////////////////////////////////////
// clearBuffer()
//
// Clears the samples in the buffer.
//
bool QwDevQMA6100P::clearBuffer()
{
  int retVal;
  uint8_t clear = 1;

  retVal = writeRegisterByte(SFE_QMA6100P_BUF_CLEAR, clear);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// runCommandTest()
//
// Runs the command test which verifies the circuitry connected to
// the accelerometer.
//
bool QwDevQMA6100P::runCommandTest()
{

  uint8_t tempVal;
  int retVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl2_bitfield_t cntl2;
  cntl2.all = tempVal;
  cntl2.bits.cotc = 1; // This is a long winded, but definitive way of setting the COTC bit
  tempVal = cntl2.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL2, tempVal); // Start the test

  if (retVal != 0)
    return false;

  retVal = readRegisterRegion(SFE_QMA6100P_COTR, &tempVal, 1); // Check COTR is 0xAA

  if (retVal != 0)
    return false;

  if (tempVal != 0xAA)
    return false;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL2, &tempVal, 1);

  if (retVal != 0)
    return false;

  cntl2.all = tempVal;
  if (cntl2.bits.cotc != 0) // Check the COTC bit has been cleared
    return false;

  retVal = readRegisterRegion(SFE_QMA6100P_COTR, &tempVal, 1);

  if (retVal != 0)
    return false;

  if (tempVal != 0x55) // Check COTR is 0x55
    return false;

  return true;
}

//////////////////////////////////////////////////
// getRawAccelData()
//
// Retrieves the raw register or buffer values representing accelerometer data.
//
// This method is 'slow' in that it checks first if the buffer is being used - every time -
// and then calls the appropriate method to read data from the buffer or the regular
// output registers. You can speed up the read by calling the read method directly,
// if you already know if the buffer is being used or not.
//
// Note: these methods do not check if the buffer / registers contain valid data.
// The user needs to do that externally by calling getSampleLevel or dataReady
// or using the INT pins to indicate that data is ready.
//
// Parameter:
// *rawAccelData - a pointer to the data struct that holds acceleromter X/Y/Z data.
//
bool QwDevQMA6100P::getRawAccelData(rawOutputData *rawAccelData)
{

  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1); // bufCntl2.bits.bufe indicates if the buffer is enabled

  if (retVal != 0)
    return false;

  sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
  bufCntl2.all = tempVal;

  if (bufCntl2.bits.bufe) // If Buffer is enabled, read there.
    return (getRawAccelBufferData(rawAccelData, (int)bufCntl2.bits.bres));
  else
    return (getRawAccelRegisterData(rawAccelData));
}

//////////////////////////////////////////////////
// getRawAccelRegisterData()
//
// Retrieves the raw register values representing accelerometer data.
//
// Note: this method does not check if the registers contain valid data.
// The user needs to do that externally by calling dataReady
// or using the INT pins to indicate that data is ready.
//
// Parameter:
// *rawAccelData - a pointer to the data struct that holds acceleromter X/Y/Z data.
//
bool QwDevQMA6100P::getRawAccelRegisterData(rawOutputData *rawAccelData)
{

  int retVal;
  uint8_t tempRegData[6] = {0};

  retVal = readRegisterRegion(SFE_QMA6100P_XOUT_L, tempRegData, TOTAL_ACCEL_DATA_16BIT); // Read 3 * 16-bit

  if (retVal != 0)
    return false;

  rawAccelData->xData = tempRegData[XLSB];
  rawAccelData->xData |= (uint16_t)tempRegData[XMSB] << 8;
  rawAccelData->yData = tempRegData[YLSB];
  rawAccelData->yData |= (uint16_t)tempRegData[YMSB] << 8;
  rawAccelData->zData = tempRegData[ZLSB];
  rawAccelData->zData |= (uint16_t)tempRegData[ZMSB] << 8;

  return true;
}

//////////////////////////////////////////////////
// getRawAccelBufferData()
//
// Retrieves the raw buffer values representing accelerometer data.
// 
// If sixteenBit is -1 (the default), the code reads the Buffer Control Register 2 bres
// bit to determine if the buffer data is 8-bit or 16-bit. You can speed up the code
// by setting sixteenBit to: 0 for 8-bit data; 1 for 16-bit data.
//
// Note: theis method does not check if the buffer contains valid data.
// The user needs to do that externally by calling getSampleLevel
// or using the INT pins to indicate that data is ready.
//
// Parameter:
// *rawAccelData - a pointer to the data struct that holds acceleromter X/Y/Z data.
// sixteenBit - defaults to -1. Set to 0 to read 8-bit data. Set to 1 to read 16-bit data.
//
bool QwDevQMA6100P::getRawAccelBufferData(rawOutputData *rawAccelData, int sixteenBit)
{

  int retVal;
  uint8_t tempRegData[6] = {0};
  bool is16bit;

  if (sixteenBit == 0) // Do we know the data is 8-bit?
  {
    is16bit = false;
  }
  else if (sixteenBit == 1) // Do we know the data is 16-bit?
  {
    is16bit = true;
  }
  else if (sixteenBit == -1) // Need to manually check the resolution
  {
    uint8_t tempVal;
    retVal = readRegisterRegion(SFE_QMA6100P_BUF_CNTL2, &tempVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_buf_cntl2_bitfield_t bufCntl2;
    bufCntl2.all = tempVal;
    is16bit = bufCntl2.bits.bres; // bufCntl2.bits.bufe indicates if the buffer is enabled
  }
  else
  {
    return false; // Can't determine the resolution
  }

  if (is16bit) // If the buffer contains 16-bit samples
    retVal = readRegisterRegion(SFE_QMA6100P_BUF_READ, tempRegData, TOTAL_ACCEL_DATA_16BIT); // Read 3 * 16-bit
  else
    retVal = readRegisterRegion(SFE_QMA6100P_BUF_READ, tempRegData, TOTAL_ACCEL_DATA_8BIT); // Read 3 * 8-bit

  if (retVal != 0)
    return false;

  if (is16bit) // Process buffer 8-bit samples
  {
    rawAccelData->xData = tempRegData[XLSB];
    rawAccelData->xData |= (uint16_t)tempRegData[XMSB] << 8;
    rawAccelData->yData = tempRegData[YLSB];
    rawAccelData->yData |= (uint16_t)tempRegData[YMSB] << 8;
    rawAccelData->zData = tempRegData[ZLSB];
    rawAccelData->zData |= (uint16_t)tempRegData[ZMSB] << 8;
  }
  else
  {
    rawAccelData->xData = 0;
    rawAccelData->xData |= (uint16_t)tempRegData[0] << 8; // Convert 8-bit signed to 16-bit signed
    rawAccelData->yData = 0;
    rawAccelData->yData |= (uint16_t)tempRegData[1] << 8;
    rawAccelData->zData = 0;
    rawAccelData->zData |= (uint16_t)tempRegData[2] << 8;
  }

  return true;
}

//////////////////////////////////////////////////
// forceSleep()
//
// Forces the accelerometer into a sleep state.
//
bool QwDevQMA6100P::forceSleep()
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL5, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl5_bitfield_t cntl5;
  cntl5.all = tempVal;
  cntl5.bits.man_sleep = 1; // Set the manual sleep bit
  tempVal = cntl5.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL5, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////
// forceWake()
//
// Forces the accelerometer into a sleep state.
//
bool QwDevQMA6100P::forceWake()
{
  int retVal;
  uint8_t tempVal;

  retVal = readRegisterRegion(SFE_QMA6100P_CNTL5, &tempVal, 1);

  if (retVal != 0)
    return false;

  sfe_qma6100p_cntl5_bitfield_t cntl5;
  cntl5.all = tempVal;
  cntl5.bits.man_wake = 1; // Set the manual wake bit
  tempVal = cntl5.all;

  retVal = writeRegisterByte(SFE_QMA6100P_CNTL5, tempVal);

  if (retVal != 0)
    return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////////
// readRegisterRegion()
//
// Calls sfebus read function.
//
//  Parameter:
//  reg- register to read from
//  data- array to store data in
//  length- Size of data in bytes (8 bits): 2 bytes = length of two
//  retval- -1 = error, 0 = success
//
int QwDevQMA6100P::readRegisterRegion(uint8_t reg, uint8_t *data, uint16_t len)
{
  return (_sfeBus->readRegisterRegion(_i2cAddress, reg, data, len));
}

//////////////////////////////////////////////////////////////////////////////////
// writeRegisterRegion()
//
// Calls sfebus write function.
//
//  Parameter:
//  reg- register to read from
//  data- array to store data in
//  length- Size of data in bytes (8 bits): 2 bytes = length of two
//  retval- -1 = error, 0 = success
//
int QwDevQMA6100P::writeRegisterRegion(uint8_t reg, uint8_t *data, uint16_t len)
{
  return (_sfeBus->writeRegisterRegion(_i2cAddress, reg, data, len));
}

//////////////////////////////////////////////////////////////////////////////////
// writeRegisterByte()
//
// Calls sfebus write function.
//
//  Parameter:
//  reg- register to read from
//  data- array to store data in
//  length- Size of data in bytes (8 bits): 2 bytes = length of two
//  retval- -1 = error, 0 = success
//
int QwDevQMA6100P::writeRegisterByte(uint8_t reg, uint8_t data)
{
  return (_sfeBus->writeRegisterByte(_i2cAddress, reg, data) ? 0 : -1);
}


//***************************************** QMA6100P ******************************************************

//////////////////////////////////////////////////////////////////////////////////
// init()
//
// Ensures that communication is established with the accelerometer by pinging its
// address and retrieving its device ID.
//
bool QwDevQMA6100P::init(void)
{
  if (!_sfeBus->ping(_i2cAddress))
    return false;

  if (getUniqueID() != QMA6100P_CHIP_ID)
    return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////////
// getAccelData()
//
// Retrieves the raw accelerometer data and calls a conversion function to convert the raw values.
//
// Parameter:
// *userData - a pointer to the user's data struct that will hold acceleromter data.
//
bool QwDevQMA6100P::getAccelData(outputData *userData)
{

  bool retVal;

  retVal = getRawAccelData(&rawAccelData);

  if (!retVal)
    return false;

  retVal = convAccelData(userData, &rawAccelData);

  if (!retVal)
    return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////////
// convAccelData()
//
// Converts raw acceleromter data with the current accelerometer's range settings.
//
// Parameter:
// *userData - a pointer to the user's data struct that will hold acceleromter data.
// *rawAccelData - a pointer to the data struct that holds acceleromter X/Y/Z data.
//
bool QwDevQMA6100P::convAccelData(outputData *userAccel, rawOutputData *rawAccelData)
{
  if (_range < 0) // If the G-range is unknown, read it
  {
    uint8_t regVal;
    int retVal;

    retVal = readRegisterRegion(SFE_QMA6100P_CNTL1, &regVal, 1);

    if (retVal != 0)
      return false;

    sfe_qma6100p_cntl1_bitfield_t cntl1;
    cntl1.all = regVal;

    _range = cntl1.bits.gsel; // Record the range
  }

  switch (_range)
  {
  case SFE_QMA6100P_RANGE8G:
    userAccel->xData = (float)rawAccelData->xData * convRange8G;
    userAccel->yData = (float)rawAccelData->yData * convRange8G;
    userAccel->zData = (float)rawAccelData->zData * convRange8G;
    break;
  case SFE_QMA6100P_RANGE16G:
    userAccel->xData = (float)rawAccelData->xData * convRange16G;
    userAccel->yData = (float)rawAccelData->yData * convRange16G;
    userAccel->zData = (float)rawAccelData->zData * convRange16G;
    break;
  case SFE_QMA6100P_RANGE32G:
    userAccel->xData = (float)rawAccelData->xData * convRange32G;
    userAccel->yData = (float)rawAccelData->yData * convRange32G;
    userAccel->zData = (float)rawAccelData->zData * convRange32G;
    break;
  case SFE_QMA6100P_RANGE64G:
    userAccel->xData = (float)rawAccelData->xData * convRange64G;
    userAccel->yData = (float)rawAccelData->yData * convRange64G;
    userAccel->zData = (float)rawAccelData->zData * convRange64G;
    break;
  default:
    return false;
  }

  return true;
}
