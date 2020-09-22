//-----------------------------------------------------------------------------
// Copyright (C) 2020 tharexde
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency EM4x50 commands
//-----------------------------------------------------------------------------

#include "BigBuf.h"
#include "fpgaloader.h"
#include "ticks.h"
#include "dbprint.h"
#include "lfadc.h"
#include "lfsampling.h"
#include "commonutil.h"
#include "em4x50.h"

// 4 data bytes
// + byte with row parities
// + column parity byte
// + byte with stop bit

static em4x50_tag_t tag = {
    .sectors = {
        [0]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // password
        [1]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // protection word
        [2]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // control word
        [3]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [4]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [5]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [6]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [7]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [8]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [9]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [10] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [11] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [12] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [13] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [14] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [15] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [17] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [18] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [19] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [20] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [21] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [22] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [23] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [24] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [25] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [26] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [27] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [28] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [29] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [30] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [31] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // user
        [32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // device serial number
        [33] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // device identification
    },
};

// Sam7s has several timers, we will use the source TIMER_CLOCK1 (aka AT91C_TC_CLKS_TIMER_DIV1_CLOCK)
// TIMER_CLOCK1 = MCK/2, MCK is running at 48 MHz, Timer is running at 48/2 = 24 MHz
// EM4x50 units (T0) have duration of 8 microseconds (us), which is 1/125000 per second (carrier)
// T0 = TIMER_CLOCK1 / 125000 = 192

#ifndef T0
#define T0                                  192
#endif

#define EM4X50_T_TAG_QUARTER_PERIOD         16
#define EM4X50_T_TAG_HALF_PERIOD            32
#define EM4X50_T_TAG_THREE_QUARTER_PERIOD   48
#define EM4X50_T_TAG_FULL_PERIOD            64
#define EM4X50_T_TAG_THREE_HALF_PERIOD      96
#define EM4X50_T_TAG_TPP                    64
#define EM4X50_T_TAG_TWA                    64
#define EM4X50_T_WAITING_FOR_SNGLLIW        100
#define EM4X50_T_WAITING_FOR_DBLLIW         1550

#define EM4X50_TAG_TOLERANCE                8
#define EM4X50_TAG_WORD                     45
#define EM4X50_SAMPLE_CNT_MAX               3000

#define EM4X50_BIT_0                        0
#define EM4X50_BIT_1                        1
#define EM4X50_BIT_OTHER                    2

#define EM4X50_COMMAND_REQUEST              2
#define EM4X50_COMMAND_LOGIN                0x01
#define EM4X50_COMMAND_RESET                0x80
#define EM4X50_COMMAND_WRITE                0x12
#define EM4X50_COMMAND_WRITE_PASSWORD       0x11
#define EM4X50_COMMAND_SELECTIVE_READ       0x0A

#define EM4X50_COMMAND_TIMEOUT              5000
#define FPGA_TIMER_0                        0

int gHigh = 0;
int gLow = 0;

// auxiliary functions

static void init_tag(void) {

    // initialize global tag structure
    for (int i = 0; i < 34; i++)
        for (int j = 0; j < 7; j++)
            tag.sectors[i][j] = 0x00;
}

static uint8_t bits2byte(uint8_t *bits, int length) {

    // converts <length> separate bits into a single "byte"
    uint8_t byte = 0;
    for (int i = 0; i < length; i++) {

        byte |= bits[i];

        if (i != length - 1)
            byte <<= 1;
    }

    return byte;
}

static void msb2lsb_word(uint8_t *word) {

    // reorders given <word> according to EM4x50 datasheet (msb -> lsb)

    uint8_t buff[4];
    buff[0] = reflect8(word[3]);
    buff[1] = reflect8(word[2]);
    buff[2] = reflect8(word[1]);
    buff[3] = reflect8(word[0]);

    word[0] = buff[0];
    word[1] = buff[1];
    word[2] = buff[2];
    word[3] = buff[3];
}

static void save_word(int pos, uint8_t bits[EM4X50_TAG_WORD]) {

    // split "raw" word into data, row and column parity bits and stop bit and
    // save them in global tag structure
    uint8_t row_parity[4];
    uint8_t col_parity[8];

    // data and row parities
    for (int i = 0; i < 4; i++) {
        tag.sectors[pos][i] = bits2byte(&bits[9 * i], 8);
        row_parity[i] = bits[9 * i + 8];
    }

    tag.sectors[pos][4] = bits2byte(row_parity, 4);

    // column parities
    for (int i = 0; i < 8; i++)
        col_parity[i] = bits[36 + i];

    tag.sectors[pos][5] = bits2byte(col_parity, 8);

    // stop bit
    tag.sectors[pos][6] = bits[44];
}

static void wait_timer(int timer, uint32_t period) {

    // do nothing for <period> using timer <timer>

    if (timer == FPGA_TIMER_0) {

        AT91C_BASE_TC0->TC_CCR = AT91C_TC_SWTRG;
        while (AT91C_BASE_TC0->TC_CV < period);
            
    } else {

        AT91C_BASE_TC1->TC_CCR = AT91C_TC_SWTRG;
        while (AT91C_BASE_TC1->TC_CV < period);

    }
}

static void em4x50_setup_read(void) {

    FpgaDownloadAndGo(FPGA_BITSTREAM_LF);
    FpgaWriteConfWord(FPGA_MAJOR_MODE_LF_ADC | FPGA_LF_ADC_READER_FIELD);

    // 50ms for the resonant antenna to settle.
    SpinDelay(50);
    // Now set up the SSC to get the ADC samples that are now streaming at us.
    FpgaSetupSsc(FPGA_MAJOR_MODE_LF_READER);
    // start a 1.5ticks is 1us
    StartTicks();

    FpgaSendCommand(FPGA_CMD_SET_DIVISOR, LF_DIVISOR_125);

    // Connect the A/D to the peak-detected low-frequency path.
    SetAdcMuxFor(GPIO_MUXSEL_LOPKD);

    // Steal this pin from the SSP (SPI communication channel with fpga) and
    // use it to control the modulation
    AT91C_BASE_PIOA->PIO_PER = GPIO_SSC_DOUT;
    AT91C_BASE_PIOA->PIO_OER = GPIO_SSC_DOUT;

    // Disable modulation at default, which means enable the field
    LOW(GPIO_SSC_DOUT);

    // Enable Peripheral Clock for
    //   TIMER_CLOCK0, used to measure exact timing before answering
    //   TIMER_CLOCK1, used to capture edges of the tag frames
    AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_TC0) | (1 << AT91C_ID_TC1);
    AT91C_BASE_PIOA->PIO_BSR = GPIO_SSC_FRAME;

    // Disable timer during configuration
    AT91C_BASE_TC0->TC_CCR = AT91C_TC_CLKDIS;
    AT91C_BASE_TC1->TC_CCR = AT91C_TC_CLKDIS;

    // TC0: Capture mode, default timer source = MCK/2 (TIMER_CLOCK1), no triggers
    AT91C_BASE_TC0->TC_CMR = AT91C_TC_CLKS_TIMER_DIV1_CLOCK;

    // TC1: Capture mode, default timer source = MCK/2 (TIMER_CLOCK1), no triggers
    AT91C_BASE_TC1->TC_CMR = AT91C_TC_CLKS_TIMER_DIV1_CLOCK;

    // Enable and reset counters
    AT91C_BASE_TC0->TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;
    AT91C_BASE_TC1->TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;

    // synchronized startup procedure
    while (AT91C_BASE_TC0->TC_CV > 0) {}; // wait until TC1 returned to zero

    // Watchdog hit
    WDT_HIT();
}

// functions for "reader" use case

static bool get_signalproperties(void) {

    // calculate signal properties (mean amplitudes) from measured data:
    // 32 amplitudes (maximum values) -> mean amplitude value -> gHigh -> gLow

    bool signal_found = false;
    int no_periods = 32, pct = 75, noise = 140;
    uint8_t sample = 0;
    uint8_t sample_ref = 127;
    uint8_t sample_max_mean = 0;
    uint8_t sample_max[no_periods];
    uint32_t sample_max_sum = 0;
    memcpy(sample_max, 0x00, sizeof(sample_max));

    // wait until signal/noise > 1 (max. 32 periods)
    for (int i = 0; i < T0 * no_periods; i++) {

        if (BUTTON_PRESS()) return false;
        
        // about 2 samples per bit period
        wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);

        if (AT91C_BASE_SSC->SSC_RHR > noise) {
            signal_found = true;
            break;
        }

    }

    if (!signal_found)
        return false;

    // calculate mean maximum value of 32 periods, each period has a length of
    // 3 single "full periods" to eliminate the influence of a listen window
    for (int i = 0; i < no_periods; i++) {

        AT91C_BASE_TC0->TC_CCR = AT91C_TC_SWTRG;
        while (AT91C_BASE_TC0->TC_CV < T0 * 3 * EM4X50_T_TAG_FULL_PERIOD) {

            if (BUTTON_PRESS()) return false;

            sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;
            if (sample > sample_max[i])
                sample_max[i] = sample;

        }

        sample_max_sum += sample_max[i];
    }

    sample_max_mean = sample_max_sum / no_periods;

    // set global envelope variables
    gHigh = sample_ref + pct * (sample_max_mean - sample_ref) / 100;
    gLow = sample_ref - pct * (sample_max_mean - sample_ref) / 100;
    return true;
}

static int get_next_bit(void) {

    // returns bit value (or EM4X50_BIT_OTHER -> no bit pattern) by evaluating
    // a single sample within a bit period (given there is no LIW, ACK or NAK)
    // This function is not used for decoding, it is only used for identifying
    // a listen window (return value = EM4X50_BIT_OTHER) in functions
    // "find_double_listen_window" and "check_ack"

    // get sample at 3/4 of bit period
    wait_timer(0, T0 * EM4X50_T_TAG_THREE_QUARTER_PERIOD);
    uint8_t sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;

    // wait until end of bit period
    wait_timer(0, T0 * EM4X50_T_TAG_QUARTER_PERIOD);

    // decide wether "0" or "1"
    if (sample > gHigh)
        return EM4X50_BIT_0;
    else if (sample < gLow)
        return EM4X50_BIT_1;

    return EM4X50_BIT_OTHER;
}

static uint32_t get_pulse_length(void) {

    // iterates pulse length (low -> high -> low)
    // to avoid endless loops - quit after EM4X50_SAMPLE_CNT_MAX samples
    
    int sample_cnt = 0;
    uint8_t sample = 0;

    sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;
    
    while (sample > gLow) {
        sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;
        if (++sample_cnt > EM4X50_SAMPLE_CNT_MAX)
            return 0;
    }

    AT91C_BASE_TC1->TC_CCR = AT91C_TC_SWTRG;

    sample_cnt = 0;
    while (sample < gHigh) {
        sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;
        if (++sample_cnt > EM4X50_SAMPLE_CNT_MAX)
            return 0;
    }

    sample_cnt = 0;
    while (sample > gLow) {
        sample = (uint8_t)AT91C_BASE_SSC->SSC_RHR;
        if (++sample_cnt > EM4X50_SAMPLE_CNT_MAX)
            return 0;
    }

    return (uint32_t)AT91C_BASE_TC1->TC_CV;
}

static bool check_pulse_length(uint32_t pl, int length) {

    // check if pulse length <pl> corresponds to given length <length>
    
    if ((pl >= T0 * (length - EM4X50_TAG_TOLERANCE)) &&
        (pl <= T0 * (length + EM4X50_TAG_TOLERANCE)))

        return true;
    else
        return false;
}

static void em4x50_reader_send_bit(int bit) {
    
    // send single bit according to EM4x50 application note and datasheet

    // reset clock for the next bit
    AT91C_BASE_TC0->TC_CCR = AT91C_TC_SWTRG;

    if (bit == 0) {

        // disable modulation (drop the field) for 7 cycles of carrier
        // period (Opt64)
        LOW(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC0->TC_CV < T0 * 7);

        // enable modulation (activates the field) for remaining first
        // half of bit period
        HIGH(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC0->TC_CV < T0 * EM4X50_T_TAG_HALF_PERIOD);

        // disable modulation for second half of bit period
        LOW(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC0->TC_CV < T0 * EM4X50_T_TAG_FULL_PERIOD);

    } else {

        // bit = "1" means disable modulation for full bit period
        LOW(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC0->TC_CV < T0 * EM4X50_T_TAG_FULL_PERIOD);
    }
}
/*
static void em4x50_sim_send_bit(int bit) {
    
    // send single bit according to EM4x50 application note and datasheet
    
    // reset clock for the next bit
    AT91C_BASE_TC1->TC_CCR = AT91C_TC_SWTRG;

    if (bit == 0) {

        // disable modulation (activates the field) for first half of bit period
        LOW(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC1->TC_CV < T0 * EM4X50_T_TAG_HALF_PERIOD);
        
        // enable modulation for second half of bit period
        HIGH(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC1->TC_CV < T0 * EM4X50_T_TAG_FULL_PERIOD);

    } else {
        
        // enable modulation (drop the field) for first half of bit period
        HIGH(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC1->TC_CV < T0 * EM4X50_T_TAG_HALF_PERIOD);
        
        // disable modulation for second half of bit period
        LOW(GPIO_SSC_DOUT);
        while (AT91C_BASE_TC1->TC_CV < T0 * EM4X50_T_TAG_FULL_PERIOD);
    }
}
*/
static void em4x50_reader_send_byte(uint8_t byte) {

    // send byte (without parity)
    
    for (int i = 0; i < 8; i++)
        em4x50_reader_send_bit((byte >> (7-i)) & 1);

}
/*
static void em4x50_sim_send_byte(uint8_t byte) {

    // send byte (without parity)

    for (int i = 0; i < 8; i++)
        em4x50_sim_send_bit((byte >> (7-i)) & 1);

}
*/
static void em4x50_reader_send_byte_with_parity(uint8_t byte) {

    // send byte followed by its (equal) parity bit
    
    int parity = 0, bit = 0;
    
    for (int i = 0; i < 8; i++) {
        bit = (byte >> (7-i)) & 1;
        em4x50_reader_send_bit(bit);
        parity ^= bit;
    }

    em4x50_reader_send_bit(parity);
}
/*
static void em4x50_sim_send_byte_with_parity(uint8_t byte) {

    // send byte followed by its (equal) parity bit

    int parity = 0, bit = 0;

    for (int i = 0; i < 8; i++) {
        bit = (byte >> (7-i)) & 1;
        em4x50_sim_send_bit(bit);
        parity ^= bit;
    }

    em4x50_sim_send_bit(parity);
}
*/
static void em4x50_reader_send_word(const uint8_t bytes[4]) {
    
    // send 32 bit word with parity bits according to EM4x50 datasheet
    
    for (int i = 0; i < 4; i++)
        em4x50_reader_send_byte_with_parity(bytes[i]);
    
    // send column parities
    em4x50_reader_send_byte(bytes[0] ^ bytes[1] ^ bytes[2] ^ bytes[3]);

    // send final stop bit (always "0")
    em4x50_reader_send_bit(0);
}
/*

static void em4x50_sim_send_word(const uint8_t bytes[4]) {
    
    // send 32 bit word with parity bits according to EM4x50 datasheet

    for (int i = 0; i < 4; i++)
        em4x50_sim_send_byte_with_parity(bytes[i]);
    
    // send column parities
    em4x50_sim_send_byte(bytes[0] ^ bytes[1] ^ bytes[2] ^ bytes[3]);

    // send final stop bit (always "0")
    em4x50_sim_send_bit(0);
}

static void em4x50_sim_send_ack(void) {
    
    // send "acknowledge" according to EM4x50 application note and datasheet
    
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_THREE_HALF_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_THREE_HALF_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);

}

static void em4x50_sim_send_nak(void) {
    
    // send "not" acknowledge" according to EM4x50 application note and datasheet
    
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_THREE_HALF_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_FULL_PERIOD);
    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);

}

static void em4x50_sim_handle_command(void) {
    
    em4x50_sim_send_ack();
    em4x50_sim_send_nak();
    Dbprintf("");
    
}
*/
/*
static bool em4x50_sim_detect_rm(void) {
    
    if (get_next_bit() == EM4X50_BIT_0)
        if (get_next_bit() == EM4X50_BIT_0)
            return true;
    
    //int periods = 0;
    //AT91C_BASE_TC0->TC_CCR = AT91C_TC_SWTRG;
    //while (AT91C_BASE_TC0->TC_CV < T0 * EM4X50_T_TAG_FULL_PERIOD)
    //    adc_val[periods++] = AT91C_BASE_SSC->SSC_RHR;

    return false;
}
*/
/*
static void em4x50_sim_send_listen_window(void) {
    
    // send single listen window according to EM4x50 application note and datasheet
    
    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);

    HIGH(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_HALF_PERIOD);

    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * 2 * EM4X50_T_TAG_FULL_PERIOD);

    HIGH(GPIO_SSC_DOUT);
    if (em4x50_sim_detect_rm())
        em4x50_sim_handle_command();

    LOW(GPIO_SSC_DOUT);
    wait_timer(0, T0 * EM4X50_T_TAG_FULL_PERIOD);
    
}
*/

static bool find_single_listen_window(void) {

    // find single listen window

    int cnt_pulses = 0;

    while (cnt_pulses < EM4X50_T_WAITING_FOR_SNGLLIW) {

        // identification of listen window is done via evaluation of
        // pulse lengths
        if (check_pulse_length(get_pulse_length(), 3 * EM4X50_T_TAG_FULL_PERIOD)) {

            if (check_pulse_length(get_pulse_length(), 2 * EM4X50_T_TAG_FULL_PERIOD)) {

                // listen window found
                return true;

            }
        } else {
            cnt_pulses++;
        }
    }

    return false;
}

static bool find_double_listen_window(bool bcommand) {

    // find two successive listen windows that indicate the beginning of
    // data transmission
    // double listen window to be detected within 1600 pulses -> worst case
    // reason: first detectable double listen window after 34 words
    // -> 34 words + 34 single listen windows -> about 1600 pulses

    int cnt_pulses = 0;

    while (cnt_pulses < EM4X50_T_WAITING_FOR_DBLLIW) {

        // identification of listen window is done via evaluation of
        // pulse lengths
        if (check_pulse_length(get_pulse_length(), 3 * EM4X50_T_TAG_FULL_PERIOD)) {

            if (check_pulse_length(get_pulse_length(), 2 * EM4X50_T_TAG_FULL_PERIOD)) {

                // first listen window found

                if (bcommand) {

                    // data transmission from card has to be stopped, because
                    // a commamd shall be issued

                    // unfortunately the posititon in listen window (where
                    // command request has to be sent) has gone, so if a
                    // second window follows - sync on this to issue a command

                    // skip the next bit...
                    wait_timer(FPGA_TIMER_0, T0 * EM4X50_T_TAG_FULL_PERIOD);

                    // ...and check if the following bit does make sense
                    // (if not it is the correct position within the second
                    // listen window)
                    if (get_next_bit() == EM4X50_BIT_OTHER) {

                        // send RM for request mode
                        em4x50_reader_send_bit(0);
                        em4x50_reader_send_bit(0);

                        return true;
                    }

                }

                if (check_pulse_length(get_pulse_length(), 3 * EM4X50_T_TAG_FULL_PERIOD)) {

                    // return although second listen window consists of one
                    // more bit period but this period is necessary for
                    // evaluating further pulse lengths
                    return true;
                }
            }
        } else {
            cnt_pulses++;
        }
    }

    return false;
}

static bool find_em4x50_tag(void) {

    // function is used to check wether a tag on the proxmark is an
    // EM4x50 tag or not -> speed up "lf search" process

    return (find_single_listen_window());

}

static bool request_receive_mode(void) {

    // To issue a command we have to find a listen window first.
    // Because identification and sychronization at the same time is not
    // possible when using pulse lengths a double listen window is used.

    bool bcommand = true;

    return find_double_listen_window(bcommand);
}

static bool check_ack(bool bliw) {

    // returns true if signal structue corresponds to ACK, anything else is
    // counted as NAK (-> false)
    // Only relevant for pasword writing function:
    // If <bliw> is true then within the single listen window right after the
    // ack signal a RM request has to be sent.

    AT91C_BASE_TC0->TC_CCR = AT91C_TC_SWTRG;
    while (AT91C_BASE_TC0->TC_CV < T0 * 4 * EM4X50_T_TAG_FULL_PERIOD) {

        if (check_pulse_length(get_pulse_length(), 2 * EM4X50_T_TAG_FULL_PERIOD)) {

            // The received signal is either ACK or NAK.

            if (check_pulse_length(get_pulse_length(), 2 * EM4X50_T_TAG_FULL_PERIOD)) {

                // Now the signal must be ACK.

                if (!bliw) {

                    return true;

                } else {

                    // send RM request after ack signal

                    // wait for 2 bits (remaining "bit" of ACK signal + first
                    // "bit" of listen window)
                    wait_timer(FPGA_TIMER_0, T0 * 2 * EM4X50_T_TAG_FULL_PERIOD);

                    // check for listen window (if first bit cannot be inerpreted
                    // as a valid bit it must belong to a listen window)
                    if (get_next_bit() == EM4X50_BIT_OTHER) {

                        // send RM for request mode
                        em4x50_reader_send_bit(0);
                        em4x50_reader_send_bit(0);

                        return true;
                    }
                }
            } else {

                // It's NAK -> stop searching
                break;
            }
        }
    }

    return false;
}

static int get_word_from_bitstream(uint8_t bits[EM4X50_TAG_WORD]) {

    // decodes one word by evaluating pulse lengths and previous bit;
    // word must have 45 bits in total:
    // 32 data bits + 4 row parity bits + 8 column parity bits + 1 stop bit

    bool bbitchange = false;
    int i = 0;
    uint32_t pl = 0;

    // initial bit value depends on last pulse length of listen window
    pl = get_pulse_length();
    if (check_pulse_length(pl, 3 * EM4X50_T_TAG_HALF_PERIOD)) {

        // pulse length = 1.5
        bits[0] = 1;

    } else if (check_pulse_length(pl, 2 * EM4X50_T_TAG_FULL_PERIOD)) {

        // pulse length = 2
        bits[0] = 0;
        bbitchange = true;

    } else {

        // pulse length = 2.5
        bits[0] = 0;
        bits[1] = 1;
        i++;
    }

    // identify remaining bits based on pulse lengths
    // between two listen windows only pulse lengths of 1, 1.5 and 2 are possible
    while (BUTTON_PRESS() == false) {
               
        i++;
        pl = get_pulse_length();

        if (check_pulse_length(pl, EM4X50_T_TAG_FULL_PERIOD)) {

            // pulse length = 1 -> keep former bit value
            bits[i] = bits[i - 1];

        } else if (check_pulse_length(pl, 3 * EM4X50_T_TAG_HALF_PERIOD)) {

            // pulse length = 1.5 -> decision on bit change

            if (bbitchange) {

                // if number of pulse lengths with 1.5 periods is even -> add bit
                bits[i] = (bits[i - 1] == 1) ? 1 : 0;

                // pulse length of 1.5 changes bit value
                bits[i + 1] = (bits[i] == 1) ? 0 : 1;
                i++;

                // next time add only one bit
                bbitchange = false;

            } else {

                bits[i] = (bits[i - 1] == 1) ? 0 : 1;

                // next time two bits have to be added
                bbitchange = true;
            }

        } else if (check_pulse_length(pl, 2 * EM4X50_T_TAG_FULL_PERIOD)) {

            // pulse length of 2 means: adding 2 bits "01"
            bits[i] = 0;
            bits[i + 1] = 1;
            i++;

        } else if (check_pulse_length(pl, 3 * EM4X50_T_TAG_FULL_PERIOD)) {

            // pulse length of 3 indicates listen window -> clear last
            // bit (= 0) and return
            return --i;

        }
    }
    
    return 0;
}

//==============================================================================
// login function
//==============================================================================

static bool login(uint8_t password[4]) {

    // simple login to EM4x50,
    // used in operations that require authentication

    if (request_receive_mode()) {

        // send login command
        em4x50_reader_send_byte_with_parity(EM4X50_COMMAND_LOGIN);

        // send password
        em4x50_reader_send_word(password);

        // check if ACK is returned
        if (check_ack(false))
            return true;

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("error in command request");
    }

    return false;
}

//==============================================================================
// reset function
//==============================================================================

static bool reset(void) {
  
    // resets EM4x50 tag (used by write function)

    if (request_receive_mode()) {

        // send login command
        em4x50_reader_send_byte_with_parity(EM4X50_COMMAND_RESET);
 
        if (check_ack(false))
            return true;

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("error in command request");
    }

    return false;
}

//==============================================================================
// read functions
//==============================================================================

static bool standard_read(int *now) {

    // reads data that tag transmits when exposed to reader field
    // (standard read mode); number of read words is saved in <now>

    int fwr = *now;
    uint8_t bits[EM4X50_TAG_WORD] = {0};

    // start with the identification of two succsessive listening windows
    if (find_double_listen_window(false)) {

        // read and save words until following double listen window is detected
        while (get_word_from_bitstream(bits) == EM4X50_TAG_WORD)
            save_word((*now)++, bits);

        // number of detected words
        *now -= fwr;

        return true;

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("didn't find a listen window");
    }

    return false;
}

static bool selective_read(uint8_t addresses[4]) {

    // reads from "first word read" (fwr = addresses[3]) to "last word read"
    // (lwr = addresses[2])
    // result is verified by "standard read mode"

    int fwr = addresses[3];     // first word read
    int lwr = addresses[2];     // last word read
    int now = fwr;              // number of words

    if (request_receive_mode()) {

        // send selective read command
        em4x50_reader_send_byte_with_parity(EM4X50_COMMAND_SELECTIVE_READ);

        // send address data
        em4x50_reader_send_word(addresses);

        // look for ACK sequence -> save and verify via standard read mode
        // (compare number of words)
        if (check_ack(false))
            if (standard_read(&now))
                if (now == (lwr - fwr + 1))
                    return true;

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("error in command request");
    }

    return false;
}

void em4x50_info(em4x50_data_t *etd) {

    // collects as much information as possible via selective read mode
    // if no password is given -> try with standard password "0x00000000"
    // otherwise continue without login

    bool bsuccess = false, blogin = false;
    uint8_t status = 0;
    uint8_t addresses[] = {0x00, 0x00, 0x21, 0x00}; // fwr = 0, lwr = 33
    uint8_t password[] = {0x00, 0x00, 0x00, 0x00};  // default password

    init_tag();
    em4x50_setup_read();

    // set gHigh and gLow
    if (get_signalproperties() && find_em4x50_tag()) {
        
        if (etd->pwd_given) {

            // try to login with given password
            blogin = login(etd->password);

        } else {

            // if no password is given, try to login with "0x00000000"
            blogin = login(password);

        }

        bsuccess = selective_read(addresses);
    }

    status = (bsuccess << 1) + blogin;

    lf_finalize();
    reply_ng(CMD_ACK, status, (uint8_t *)tag.sectors, 238);
}

void em4x50_read(em4x50_data_t *etd) {

    // reads in two different ways:
    // - using "selective read mode" -> bidirectional communication
    // - using "standard read mode" -> unidirectional communication (read
    //   data that tag transmits "voluntarily")

    bool bsuccess = false, blogin = false;
    int now = 0;
    uint8_t status = 0;
    uint8_t addresses[] = {0x00, 0x00, 0x00, 0x00};

    init_tag();
    em4x50_setup_read();

    // set gHigh and gLow
    if (get_signalproperties()) {//} && find_em4x50_tag()) {
        if (etd->addr_given) {

            // selective read mode

            // try to login with given password
            if (etd->pwd_given)
                blogin = login(etd->password);

            // only one word has to be read -> first word read = last word read
            addresses[2] = addresses[3] = etd->address;
            bsuccess = selective_read(addresses);

        } else {

            // standard read mode
            bsuccess = standard_read(&now);
        }
    }

    status = (now << 2) + (bsuccess << 1) + blogin;

    LOW(GPIO_SSC_DOUT);
    lf_finalize();
    reply_ng(CMD_ACK, status, (uint8_t *)tag.sectors, 238);
}

//==============================================================================
// write functions
//==============================================================================

static bool write(uint8_t word[4], uint8_t address) {

    // writes <word> to specified <address>

    if (request_receive_mode()) {

        // send write command
        em4x50_reader_send_byte_with_parity(EM4X50_COMMAND_WRITE);

        // send address data
        em4x50_reader_send_byte_with_parity(address);

        // send data
        em4x50_reader_send_word(word);

        // wait for T0 * EM4X50_T_TAG_TWA (write access time)
        wait_timer(FPGA_TIMER_0, T0 * EM4X50_T_TAG_TWA);

        // look for ACK sequence
        if (check_ack(false)) {

            // now EM4x50 needs T0 * EM4X50_T_TAG_TWEE (EEPROM write time)
            // for saving data and should return with ACK
            if (check_ack(false))
                return true;

        }

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("error in command request");
    }

    return false;
}

static bool write_password(uint8_t password[4], uint8_t new_password[4]) {

    // changes password from <password> to <new_password>

    if (request_receive_mode()) {

        // send write password command
        em4x50_reader_send_byte_with_parity(EM4X50_COMMAND_WRITE_PASSWORD);

        // send address data
        em4x50_reader_send_word(password);

        // wait for T0 * EM4x50_T_TAG_TPP (processing pause time)
        wait_timer(FPGA_TIMER_0, T0 * EM4X50_T_TAG_TPP);

        // look for ACK sequence and send rm request
        // during following listen window
        if (check_ack(true)) {

            // send new password
            em4x50_reader_send_word(new_password);

            // wait for T0 * EM4X50_T_TAG_TWA (write access time)
            wait_timer(FPGA_TIMER_0, T0 * EM4X50_T_TAG_TWA);

            if (check_ack(false))
                if (check_ack(false))
                    return true;

        }

    } else {
        if (DBGLEVEL >= DBG_DEBUG)
            Dbprintf("error in command request");
    }

    return false;
}

void em4x50_write(em4x50_data_t *etd) {

    // write operation process for EM4x50 tag,
    // single word is written to given address, verified by selective read operation

    bool bsuccess = false, blogin = false;
    uint8_t status = 0;
    uint8_t word[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t addresses[4] = {0x00, 0x00, 0x00, 0x00};

    init_tag();
    em4x50_setup_read();

    // set gHigh and gLow
    if (get_signalproperties() && find_em4x50_tag()) {

        // reorder word according to datasheet
        msb2lsb_word(etd->word);

        // if password is given try to login first
        if (etd->pwd_given)
            blogin = login(etd->password);

        // write word to given address
        if (write(etd->word, etd->address)) {

            // to verify result reset EM4x50
            if (reset()) {

                // if password is given login
                if (etd->pwd_given)
                    blogin &= login(etd->password);

                // call a selective read
                addresses[2] = addresses[3] = etd->address;
                if (selective_read(addresses)) {

                    // compare with given word
                    word[0] = tag.sectors[etd->address][0];
                    word[1] = tag.sectors[etd->address][1];
                    word[2] = tag.sectors[etd->address][2];
                    word[3] = tag.sectors[etd->address][3];
                    msb2lsb_word(word);

                    bsuccess = true;
                    for (int i = 0; i < 4; i++)
                        bsuccess &= (word[i] == etd->word[i]) ? true : false;

                }
            }
        }
    }

    status = (bsuccess << 1) + blogin;

    lf_finalize();
    reply_ng(CMD_ACK, status, (uint8_t *)tag.sectors, 238);
}

void em4x50_write_password(em4x50_data_t *etd) {

    // sinmple change of password

    bool bsuccess = false;

    init_tag();
    em4x50_setup_read();

    // set gHigh and gLow
    if (get_signalproperties() && find_em4x50_tag()) {

        // login and change password
        if (login(etd->password)) {
            bsuccess = write_password(etd->password, etd->new_password);
        }
    }

    lf_finalize();
    reply_ng(CMD_ACK, bsuccess, 0, 0);
}

void em4x50_wipe(em4x50_data_t *etd) {

    // set all data of EM4x50 tag to 0x0 including password

    bool bsuccess = false;
    uint8_t zero[4] = {0, 0, 0, 0};
    uint8_t addresses[4] = {0, 0, EM4X50_NO_WORDS - 3, 1};

    init_tag();
    em4x50_setup_read();

    // set gHigh and gLow
    if (get_signalproperties() && find_em4x50_tag()) {

        // login first
        if (login(etd->password)) {

            // write 0x0 to each address but ignore addresses
            // 0 -> password, 32 -> serial, 33 -> uid
            // writing 34 words takes about 3.6 seconds -> high timeout needed
            for (int i = 1; i <= EM4X50_NO_WORDS - 3; i++)
                write(zero, i);

            // to verify result reset EM4x50
            if (reset()) {

                // login not necessary because protectd word has been set to 0
                // -> no read protected words
                // -> selective read can be called immediately
                if (selective_read(addresses)) {

                    // check if everything is zero
                    bsuccess = true;
                    for (int i = 1; i <= EM4X50_NO_WORDS - 3; i++)
                        for (int j = 0; j < 4; j++)
                            bsuccess &= (tag.sectors[i][j] == 0) ? true : false;

                }

                if (bsuccess) {

                    // so far everything is fine
                    // last task: reset password
                    if (login(etd->password))
                        bsuccess = write_password(etd->password, zero);

                    // verify by login with new password
                    if (bsuccess)
                        bsuccess = login(zero);
                }
            }
        }
    }

    lf_finalize();
    reply_ng(CMD_ACK, bsuccess, (uint8_t *)tag.sectors, 238);
}

static bool em4x50_sim_send_bit2(uint8_t bit) {
    
    uint16_t check = 0;

    for (int t = 0; t < EM4X50_T_TAG_FULL_PERIOD; t++) {

        // wait until SSC_CLK goes HIGH
        // used as a simple detection of a reader field?
        while (!(AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK)) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return false;
                check = 0;
            }
            ++check;
        }

        if (bit)
            OPEN_COIL();
        else
            SHORT_COIL();

        check = 0;
        
        //wait until SSC_CLK goes LOW
        while (AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return false;
                check = 0;
            }
            ++check;
        }

        if (t == EM4X50_T_TAG_HALF_PERIOD)
            bit ^= 1;

    }
    
    return true;
}

static bool em4x50_sim_send_byte2(uint8_t byte) {

    // send byte
    for (int i = 0; i < 8; i++)
        if (!em4x50_sim_send_bit2((byte >> (7 - i)) & 1))
            return false;
    
    return true;

}

static bool em4x50_sim_send_byte_with_parity2(uint8_t byte) {

    uint8_t parity = 0x0;
    
    // send byte with parity (even)
    for (int i = 0; i < 8; i++)
        parity ^= (byte >> i) & 1;
    
    if (!em4x50_sim_send_byte2(byte))
        return false;;
    
    if (!em4x50_sim_send_bit2(parity))
        return false;
    
    return true;
}

bool em4x50_sim_send_word3(uint32_t word) {

    uint8_t cparity = 0x00;
    
    // 4 bytes each with even row parity bit
    for (int i = 0; i < 4; i++)
        if (!em4x50_sim_send_byte_with_parity2( (word >> ((3 - i) * 8)) & 0xFF))
            return false;

    // column parity
    for (int i = 0; i < 8; i++) {
        cparity <<= 1;
        for (int j = 0; j < 4; j++) {
            cparity ^= (((word >> ((3 - j) * 8)) & 0xFF) >> (7 - i)) & 1;
        }
    }
    if (!em4x50_sim_send_byte2(cparity))
        return false;
    
    // stop bit
    if (!em4x50_sim_send_bit2(0))
        return false;

    return true;
}

bool em4x50_sim_send_listen_window2(void) {

    //int i = 0;
    uint16_t check = 0;
    //uint8_t test[100] = {0};
    
    for (int t = 0; t < 5 * EM4X50_T_TAG_FULL_PERIOD; t++) {

        // wait until SSC_CLK goes HIGH
        while (!(AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK)) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return false;
                check = 0;
            }
            ++check;
        }
        
        if (t >= 4 * EM4X50_T_TAG_FULL_PERIOD) {
            SHORT_COIL();
        } else if (t >= 3 * EM4X50_T_TAG_FULL_PERIOD) {
            OPEN_COIL();
        } else if (t >= EM4X50_T_TAG_FULL_PERIOD) {
            SHORT_COIL();
        } else if (t >= EM4X50_T_TAG_HALF_PERIOD) {
            OPEN_COIL();
        } else {
            SHORT_COIL();
        }

        check = 0;
        
        //wait until SSC_CLK goes LOW
        while (AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return false;
                check = 0;
            }
            ++check;
        }
    }

    return true;
}

/*
static void em4x50_sim_send_word3(uint8_t *word) {
    
    uint8_t rparity = 0x00, cparity = 0x00;
    uint64_t word_with_parities = { 0x00 };
    
    // bytes + row parities
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            word_with_parities += (word[i] >> j) & 1;
            word_with_parities <<= 1;
            rparity ^= (word[i] >> j) & 1;
        }
        word_with_parities += rparity & 1;
        word_with_parities <<= 1;
    }

    // column parities
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            cparity ^= (word[j] >> i) & 1;
        }
        word_with_parities += cparity;
        word_with_parities <<= 1;
        cparity = 0;
    }
    
    // stop bit
    word_with_parities += 0;
            
    // send total word
    for (int i = 0; i < EM4X50_TAG_WORD; i++)
        em4x50_sim_send_bit2((word_with_parities >> (EM4X50_TAG_WORD-1 - i)) & 1);
    
}
*/
 
/*
static void simlf(uint8_t *buf, int period) {
    
    int i = 0, count = 0;
    int clock1 = 32, clock2 = 64;
    uint16_t check = 0;

    for (;;) {

        // wait until SSC_CLK goes HIGH
        // used as a simple detection of a reader field?
        while (!(AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK)) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return;
                check = 0;
            }
            ++check;
        }
        
        if (buf[i])
            OPEN_COIL();
        else
            SHORT_COIL();

        check = 0;
         
        //wait until SSC_CLK goes LOW
        while (AT91C_BASE_PIOA->PIO_PDSR & GPIO_SSC_CLK) {
            WDT_HIT();
            if (check == 1000) {
                if (BUTTON_PRESS())
                    return;
                check = 0;
            }
            ++check;
        }
        
        if (count == EM4X50_T_TAG_HALF_PERIOD) {
            buf[i] ^= 1;
        } else if (count == EM4X50_T_TAG_FULL_PERIOD) {
            buf[i] ^= 1;
            count = 0;
            i++;
            if (i == period) {
                i = 0;
            }
        }
        count++;
    }
}
*/

void em4x50_sim(em4x50_data_t *etd) {
    
    bool bsuccess = false;
    
    lf_finalize();
    reply_ng(CMD_ACK, bsuccess, (uint8_t *)tag.sectors, 238);
}

void em4x50_test(em4x50_data_t *etd) {
    
    bool bsuccess = true;

    reply_ng(CMD_ACK, bsuccess, (uint8_t *)tag.sectors, 238);
}

int em4x50_standalone_read(uint64_t *words) {
    
    int now = 0;
    uint8_t bits[EM4X50_TAG_WORD];
    
    em4x50_setup_read();

    if (get_signalproperties() && find_em4x50_tag()) {

        if (find_double_listen_window(false)) {

            memset(bits, 0, sizeof(bits));

            while (get_word_from_bitstream(bits) == EM4X50_TAG_WORD) {

                words[now] = 0;
               
                for (int i = 0; i < EM4X50_TAG_WORD; i++) {
                    words[now] <<= 1;
                    words[now] += bits[i] & 1;
                }

                now++;
            }
        }
    }

    return now;
}
