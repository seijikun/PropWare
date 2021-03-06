/**
 * @file        PropWare/gpio/pin.h
 *
 * @author      David Zemon
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

#pragma once

#include <PropWare/PropWare.h>
#include <PropWare/gpio/port.h>

namespace PropWare {

/**
 * @brief   Utility class to handle general purpose I/O pins
 */
class Pin : public Port {
    public:
        enum class Channel {
                A,
                B
        };

    public:
        /**
         * @brief       Great for quick debugging to ensure a line of code is executed, this will quickly flash a given
         *              pin a specific number of times
         *
         * @param[in]   pinMask     Pin that should be flashed
         * @param[in]   iterations  Number of times that the pin should flicker on and back off again
         */
        static void flash_pin (const Pin::Mask pinMask, const unsigned int iterations = 10) {
            flash_port(pinMask, iterations);
        }

    public:
        /**
         * @brief       Create a Pin variable
         *
         * @param[in]   mask    Bit-mask of pin; One of PropWare::Pin::Mask
         */
        Pin (const PropWare::Pin::Mask mask = NULL_PIN)
                : Port(mask),
                  m_channel(Channel::A) {
        }

        /**
         * @param[in]   mask        Bit-mask of pin; One of PropWare::Pin::Mask
         * @param[in]   direction   Direction to initialize pin; One of PropWare::Pin::Dir
         */
        Pin (const Pin::Mask mask, const Pin::Dir direction)
                : Port(mask, direction),
                  m_channel(Channel::A) {
        }

        /**
         * @see PropWare::Port::set_mask()
         */
        void set_mask (const Pin::Mask mask) {
            this->Port::set_mask(mask);
        }

        /**
         * @brief       Set a Pin's mask based on the pin number (an integer, 0 through 31)
         *
         * @param[in]   pinNum  An integer 0-31 representing GPIO pins P0-P31
         */
        void set_pin_num (const uint8_t pinNum) {
            if (31 <= pinNum)
                this->m_mask = Mask::NULL_PIN;
            else
                this->m_mask = (uint32_t) (1 << pinNum);
        }

        Pin::Mask get_mask () const {
            return static_cast<Pin::Mask>(this->m_mask);
        }

        Channel get_channel () const {
            return this->m_channel;
        }

        void set_channel (const Channel channel) {
            this->m_channel = channel;
        }

        /**
         * @brief       Set or clear the pin programmatically
         *
         * @param[in]   True for a high pin, false for a low pin
         */
        void write (const bool value) const {
            if (value)
                this->set();
            else
                this->clear();
        }

        /**
         * @brief   Read the value from a single pin and return its state
         *
         * @return  True if the pin is high, False if the pin is low
         */
        bool read () const {
            return static_cast<bool>(this->read_fast());
        }

        /**
         * @brief   Hold cog execution until an input pin goes high
         *
         * @pre     Pin must be configured as input; You will have very sad and undesirable results if your pin is an
         *          output at the time of calling this function
         */
        void wait_until_high () const {
            waitpeq(this->m_mask, this->m_mask);
        }

        /**
         * @brief   Hold cog execution until an input pin goes low
         *
         * @pre     Pin must be configured as input; You will have very sad and undesirable results if your pin is an
         *          output at the time of calling this function
         */
        void wait_until_low () const {
            waitpeq(0, this->m_mask);
        }

        /**
         * @brief   Hold cog execution until an input pin toggles
         *
         * @pre     Pin must be configured as input; You will have very sad and undesirable results if your pin is an
         *          output at the time of calling this function
         */
        void wait_until_toggle () const {
            waitpne(this->read_fast(), this->m_mask);
        }

        /**
         * @brief       Allow easy switch-press detection of any pin; Includes de-bounce protection
         *
         * @param[in]   debounceDelayInMillis   Set the de-bounce delay in units of milliseconds
         *
         * @return      Returns 1 or 0 depending on whether the switch was pressed
         */
        bool is_switch_low (const uint16_t debounceDelayInMillis = 3) const {
            this->set_dir(Dir::IN);  // Set the pin as input

            if (!(this->read())) {   // If pin is grounded (aka, pressed)
                waitcnt(debounceDelayInMillis * MILLISECOND + CNT);

                return !(this->read());  // Check if it's still pressed
            }

            return false;
        }

        /**
         * @brief   Set to input and measure the time it takes a signal to transition from a start state to the
         *          opposite state.
         *
         * Named rc_time because it is often used to measure a resistor-capacitor circuit's tendency to "decay" to
         * either ground or 5 V (depending on wiring).  Default time increments are specified in 1 microsecond units.
         * Unit size can be changed with a call to set_io_dt function.  The pulse will be positive if the I/O pin is
         * transmitting a low signal before the call.
         *
         * @param[in]   state       Starting pin state; `true` for high, `false` for low
         * @param[in]   timeout     Time to wait (in clock ticks) before canceling the function call
         *                          [Default = 1/4 second]
         *
         * @returns     Time from starting pin (in clock ticks)
         */
        int rc_time (const bool state, const uint32_t timeout = SECOND / 4) {
            // Taken from Simple's rc_time(int pin, int state) in rcTime.C
            /*
if(iodt == 0)                               // If dt not initialized
{
  set_io_dt(CLKFREQ/1000000);               // Set up timed I/O time increment
  set_io_timeout(CLKFREQ/4);                // Set up timeout
}
*/
            uint32_t ctr = static_cast<uint32_t>((8 + ((!state & 1) * 4)) << 26);        // POS detector counter setup
            ctr += Pin::convert(static_cast<Mask>(this->m_mask));                        // Add pin to setup
            const uint32_t startTime = CNT;                                              // Mark current time
            if (CTRA == 0) {
                // If CTRA unused
                CTRA = ctr;                                                              // Configure CTRA
                FRQA = 1;                                                                // FRQA increments PHSA by 1
                this->set_dir_in();
                PHSA = 0;                                                                // Clear PHSA
                // Wait for decay or timeout
                while (state == this->read() && (CNT - startTime <= timeout));
                CTRA = 0;                                                                // Stop the counter module
                return PHSA;
            } else if (CTRB == 0) {
                // If CTRA used, try CTRB
                CTRB = ctr;                                                              // Same procedure as for CTRA
                FRQB = 1;
                this->set_dir_in();
                PHSB = 0;
                while (state == this->read() && (CNT - startTime <= timeout));
                CTRB = 0;
                return PHSB;
            } else {
                // If CTRA & CTRB in use
                return -1;
            }
        }

        /**
         * @brief   Output a PWM signal on this pin
         *
         * Use the Propeller's built-in hardware counters to generate a PWM signal on the pin. This method _does not_
         * set the pin direction, so be sure to invoke `PropWare::Pin::set_dir_out()` prior to invoking this method.
         *
         * @param[in]   frequency   Frequency in Hertz (Hz)
         */
        void start_hardware_pwm (const uint32_t frequency) const {
            this->stop_hardware_pwm();

            const uint32_t frq = static_cast<uint32_t>((UINT32_MAX + 1ULL) * frequency / CLKFREQ);
            const uint32_t ctr = (4 << 26) | static_cast<uint32_t>(convert(static_cast<Mask>(this->m_mask)));
            if (Channel::A == this->m_channel) {
                FRQA = frq;
                PHSA = 0;
                CTRA = ctr;
            } else if (Channel::B == this->m_channel) {
                FRQB = frq;
                PHSB = 0;
                CTRB = ctr;
            }
        }

        /**
         * @brief   Stop the hardware counter
         */
        void stop_hardware_pwm () const {
            if (Channel::A == this->m_channel) {
                CTRA = 0;
            } else if (Channel::B == this->m_channel) {
                CTRB = 0;
            }
        }

    private:
        /****************************************
         *** Nonsensical functions for a pin ***
         ****************************************/
        /**
         * Hide from user - should not be accessible within PropWare::Pin
         */
        void set_mask (const uint32_t mask) {
        }

        /**
         * Hide from user - should not be accessible within PropWare::Pin
         */
        void add_pins (const uint32_t mask) {
        }

        /**
         * Hide from user - should not be accessible within PropWare::Pin
         */
        uint32_t read_fast () const {
            return this->Port::read_fast();
        }

    private:
        Channel m_channel;
};

}
