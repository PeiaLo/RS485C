/*
 * Dungle_ATTINY402.ino
 *
 * CRC32 Authentication Dongle
 * MCU: ATTINY402 (8-pin SOIC)
 *
 * Function:
 *   - Receive 13-byte CRC32 challenge via UART
 *   - Compute CRC32 using shared key table
 *   - Return 4-byte CRC32 response
 *   - No bus ID, direct UART communication
 *
 * Pinout (ATTINY402, 8-pin SOIC):
 *   Pin 1: VDD (3.3V)
 *   Pin 2: PA6 (TXD) -> Comm Board RX
 *   Pin 3: PA7 (RXD) -> Comm Board TX
 *   Pin 4: PA1 (NC)
 *   Pin 5: PA2 (NC)
 *   Pin 6: PA3 (NC)
 *   Pin 7: PA0 (UPDI programming)
 *   Pin 8: GND
 *
 * UART: 9600 baud, 8N1
 *
 * Protocol (no ID, direct):
 *   RX: [0xA5][0x5A][key_sel][10B random]  (13 bytes total)
 *   TX: [CRC32 big-endian]                 (4 bytes total)
 *
 * Development:
 *   Arduino IDE + megaTinyCore
 *   Board: ATtiny402
 *   Clock: 20MHz internal
 */

/* ---- 10 Dongle 專用密鑰 (與 SlideBar 不同) ---- */
const uint32_t keyC[10] = {
    0xA37D52F1, 0xD4E8B6C3, 0x7F19AE85, 0xE2C04D97, 0x5B6F3A12,
    0x91AD7E64, 0x48C6F9D8, 0x2E75814B, 0xBC32E5A6, 0x6DA9CF73
};

/* ---- Custom CRC32 (identical to Comm/Keyboard/SlideBar) ---- */
uint32_t crc32b(const uint8_t *message, uint8_t key_idx)
{
    uint8_t  select_C = key_idx & 0x07;
    uint32_t CRC_Key  = keyC[select_C];
    uint32_t crc = 0xFFFFFFFF;

    for (uint8_t i = 0; i < 10; i++)
    {
        uint32_t mask = (uint32_t)message[i] & 0x000000FF;
        crc ^= mask;

        for (uint8_t j = 0; j < 13; j++)
        {
            if (((crc & 1) != 0) && (j != select_C))
                crc = (crc >> 1) ^ CRC_Key;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ---- Receive timeout ---- */
#define RX_TIMEOUT_MS  500

void setup()
{
    Serial.begin(9600);
}

void loop()
{
    /* Wait for first header byte */
    if (!Serial.available()) return;

    uint8_t first = Serial.read();
    if (first != 0xA5) return;

    /* Receive remaining 12 bytes with timeout */
    uint8_t buf[13];
    buf[0] = first;
    uint8_t count = 1;
    uint32_t t = millis();

    while (count < 13)
    {
        if (Serial.available())
        {
            buf[count++] = Serial.read();
            t = millis();
        }
        if ((millis() - t) > RX_TIMEOUT_MS)
            return;   /* Timeout -> discard */
    }

    /* Validate second header byte */
    if (buf[1] != 0x5A) return;

    /* Extract key_sel and 10-byte random data */
    uint8_t key_sel = buf[2] & 0x07;
    uint8_t ps_code[11];
    for (uint8_t i = 0; i < 10; i++)
        ps_code[i] = buf[3 + i];
    ps_code[10] = 0x00;

    /* Compute CRC32 */
    uint32_t crc = crc32b(ps_code, key_sel);

    /* Send 4-byte response (big-endian) */
    Serial.write((uint8_t)((crc >> 24) & 0xFF));
    Serial.write((uint8_t)((crc >> 16) & 0xFF));
    Serial.write((uint8_t)((crc >> 8)  & 0xFF));
    Serial.write((uint8_t)(crc & 0xFF));
}
