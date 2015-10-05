/**
 * @file    I2C_Demo.cpp
 *
 * @author  David Zemon
 */

#include <PropWare/i2c.h>
#include <PropWare/printer/printer.h>
#include <simple/simpletools.h>

const uint8_t MAGIC_ARRAY[] = "DCBA0";
const size_t  ARRAY_SIZE    = sizeof(MAGIC_ARRAY);

const uint8_t  SHIFTED_DEVICE_ADDR = EEPROM_ADDR << 1;
const uint16_t TEST_ADDRESS        = 32 * 1024; // Place the data immediately above the first 32k of data

int main () {
    const PropWare::I2C pwI2C;
    pwOut << "EEPROM ack = " << pwI2C.ping(SHIFTED_DEVICE_ADDR) << '\n';

    bool success = pwI2C.put(SHIFTED_DEVICE_ADDR, TEST_ADDRESS, MAGIC_ARRAY, ARRAY_SIZE);
    pwOut << "Put status: " << success << '\n';

    // Wait for write to finish
    while (!pwI2C.ping(SHIFTED_DEVICE_ADDR));

    uint8_t buffer[ARRAY_SIZE];
    success &= pwI2C.get(SHIFTED_DEVICE_ADDR, TEST_ADDRESS, buffer, ARRAY_SIZE);
    pwOut << "Get status: " << success << '\n';

    pwOut << "Returned string = '" << (char *) buffer << "'\n";

    return 0;
}