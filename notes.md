Channel hopping:

A hash function is used to calculate the channels to be hopped.
Each byte modulo 64 represents one channel.
Channel 0 is always the rendevouz (rdv) channel.

MD5     16 channels
SHA-1   20 channels
SHA-256 32 channels
SHA-512 64 channels



FLRC:
FLRC_BR_1_300_BW_1_2    Raw Bit Rate = 1.3Mbit/s
CR 3/4 => 0.975 Effective Data Rate, FLRC_CR_3_4

Fixed-Length: 127 byte payload. Makes hopping easier but corruption more likely
Assuming Variable length with 127 byte payload as worst case

4 byte AGC preamble: PREAMBLE_LENGTH_32_BITS
2 byte CRC CRC_2_BYTE

WITH SYNC WORD:
nuncoded = 32 + 21 + 32 + 16 = 101 bit
ncoded = (1016 + 16 + 6) * 4/3 = 1038 * 4/3 = 1384 bit

tbit = 1/1300000s * (101 + 1384) = 1/1300000 * 1485 = 0,00114230769230769230769231s = 1.142307ms = 1142.307µs

=> hop every 1.2ms
=> 833.33 packets/hops per second, max 127 byte payload each
=> 105791 byte payload per second

WITHOUT SYNC WORD:
nuncoded = 32 + 21 + 16 = 69 bit
ncoded = (1016 + 16 + 6) * 4/3 = 1038 * 4/3 = 1384 bit

tbit = 1/1300000s * (69 + 1384) = 1/1300000 * 1453 = 0,00111769230769230769230769s = 1.117692ms = 1117.692µs

=> hop every 1.2ms
=> 833.33 packets/hops per second, max 127 byte payload each
=> 105791 byte payload per second


FLRC_SYNC_WORD_LEN_P32S

RX_MATCH_SYNC_WORD_1

PACKET_VARIABLE_LENGTH

PAYLOAD_LENGTH = 127 (RX). Shorter  for TX



Comparison: One DMX512 universe, including BREAK and MAB takes at min 22.67ms
