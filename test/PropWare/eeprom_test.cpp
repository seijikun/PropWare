/**
 * @file    eeprom_test.cpp
 *
 * @author  David Zemon
 *
 * Hardware:
 *      Standard Propeller with an EEPROM 64 kB or greater
 *
 * @copyright
 * The MIT License (MIT)<br>
 * <br>Copyright (c) 2013 David Zemon<br>
 * <br>Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:<br>
 * <br>The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.<br>
 * <br>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "PropWareTests.h"
#include <PropWare/memory/eeprom.h>

using namespace PropWare;

Eeprom *testable;

SETUP {
    testable = new Eeprom();
};

TEARDOWN {
    if (testable)
        delete testable;
    testable = NULL;
};

TEST(Constructor_DefaultArguments) {
    setUp();

    ASSERT_EQ_MSG(&pwI2c, testable->m_driver);
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    ASSERT_EQ_MSG(Eeprom::DEFAULT_DEVICE_ADDRESS, testable->m_deviceAddress);
    ASSERT_TRUE(testable->m_autoIncrement);

    tearDown();
}

TEST(Constructor_NonDefaultArguments) {
    I2CMaster    bogus;
    Eeprom localTestable(bogus, 1, 2, false);

    ASSERT_EQ_MSG((unsigned int) &bogus, (unsigned int) localTestable.m_driver);
    ASSERT_EQ_MSG(1, localTestable.get_memory_address());
    ASSERT_EQ_MSG(2, localTestable.m_deviceAddress);
    ASSERT_FALSE(localTestable.m_autoIncrement);

    tearDown();
}

TEST(GetSetMemoryAddress) {
    setUp();

    testable->set_memory_address(0x1234);
    ASSERT_EQ_MSG(0x1234, testable->m_memoryAddress);
    ASSERT_EQ_MSG(0x1234, testable->get_memory_address());

    testable->set_memory_address(0x4321);
    ASSERT_EQ_MSG(0x4321, testable->m_memoryAddress);
    ASSERT_EQ_MSG(0x4321, testable->get_memory_address());

    tearDown();
}

TEST(GetSetAutoIncrement) {
    setUp();

    testable->set_auto_increment(false);
    ASSERT_FALSE(testable->m_autoIncrement);
    ASSERT_FALSE(testable->is_auto_increment());

    testable->set_auto_increment(true);
    ASSERT_TRUE(testable->m_autoIncrement);
    ASSERT_TRUE(testable->is_auto_increment());

    tearDown();
}

TEST(Ping) {
    setUp();

    ASSERT_TRUE(testable->ping());

    tearDown();
}

TEST(PutGet_SingleByte) {
    setUp();

    const uint8_t sampleByte1 = 0x5A;

    ASSERT_TRUE(testable->put(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, sampleByte1));
    ASSERT_EQ_MSG(sampleByte1, testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS));

    const uint8_t sampleByte2 = 0xA5;

    ASSERT_TRUE(testable->put(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, sampleByte2));
    ASSERT_EQ_MSG(sampleByte2, testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS));

    tearDown();
}

TEST(PutGet_Array) {
    setUp();

    uint8_t       buffer[64];
    const uint8_t sampleBytes1[] = "Hello";

    ASSERT_TRUE(testable->put(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, sampleBytes1, sizeof(sampleBytes1)));
    ASSERT_TRUE(testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, buffer, sizeof(sampleBytes1)));
    ASSERT_EQ_MSG(0, strcmp((char *) sampleBytes1, (char *) buffer));

    const uint8_t sampleBytes2[] = "Goodbye";

    ASSERT_TRUE(testable->put(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, sampleBytes2, sizeof(sampleBytes2)));
    ASSERT_TRUE(testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, buffer, sizeof(sampleBytes2)));
    ASSERT_EQ_MSG(0, strcmp((char *) sampleBytes2, (char *) buffer));

    tearDown();
}

TEST(PutChar_IncrementEnabled) {
    setUp();

    testable->set_auto_increment(true);

    testable->put_char('H');
    testable->put_char('e');
    testable->put_char('l');
    testable->put_char('l');
    testable->put_char('o');

    testable->set_memory_address(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS);

    ASSERT_EQ_MSG('H', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS));
    ASSERT_EQ_MSG('e', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS + 1));
    ASSERT_EQ_MSG('l', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS + 2));
    ASSERT_EQ_MSG('l', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS + 3));
    ASSERT_EQ_MSG('o', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS + 4));

    tearDown();
}

TEST(PutChar_IncrementDisabled) {
    setUp();

    testable->set_auto_increment(false);

    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    testable->put_char('H');
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    testable->put_char('e');
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    testable->put_char('l');
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    testable->put_char('l');
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    testable->put_char('o');
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());

    ASSERT_EQ_MSG('o', testable->get(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS));

    tearDown();
}

TEST(GetChar_IncrementEnabled) {
    setUp();

    testable->set_auto_increment(true);
    testable->puts("Hello");
    testable->set_memory_address(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS);

    ASSERT_EQ_MSG('H', testable->get_char());
    ASSERT_EQ_MSG('e', testable->get_char());
    ASSERT_EQ_MSG('l', testable->get_char());
    ASSERT_EQ_MSG('l', testable->get_char());
    ASSERT_EQ_MSG('o', testable->get_char());

    tearDown();
}

TEST(GetChar_IncrementDisabled) {
    setUp();

    testable->set_auto_increment(false);
    testable->puts("Hello");

    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    ASSERT_EQ_MSG('H', testable->get_char());
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());
    ASSERT_EQ_MSG('H', testable->get_char());
    ASSERT_EQ_MSG(Eeprom::DEFAULT_INITIAL_MEMORY_ADDRESS, testable->get_memory_address());

    tearDown();
}

int main() {
    START(EEPROM);

    RUN_TEST(Constructor_DefaultArguments);
    RUN_TEST(Constructor_NonDefaultArguments);
    RUN_TEST(GetSetMemoryAddress);
    RUN_TEST(GetSetAutoIncrement);
    RUN_TEST(Ping);
    RUN_TEST(PutGet_SingleByte);
    RUN_TEST(PutGet_Array);
    RUN_TEST(PutChar_IncrementEnabled);
    RUN_TEST(PutChar_IncrementDisabled);
    RUN_TEST(GetChar_IncrementEnabled);
    RUN_TEST(GetChar_IncrementDisabled);

    COMPLETE();
}
