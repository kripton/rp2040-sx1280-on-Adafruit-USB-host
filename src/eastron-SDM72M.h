#ifndef EASTRON_SDM72M_H
#define EASTRON_SDM72M_H

#include <stdint.h>

// Source:
// https://data.stromzähler.eu/manuals/eastron_sdm72dmv2.pdf

uint16_t eastron_sdm72m_input_registers[] = {
    0x0000, // Ph1 Voltage to Neutral,
    0x0002, // Ph2 Voltage to Neutral,
    0x0004, // Ph3 Voltage to Neutral,
    0x0006, // Ph1 Current,
    0x0008, // Ph2 Current,
    0x000A, // Ph3 Current,
    0x000C, // Ph1 Active Power,
    0x000E, // Ph2 Active Power,
    0x0010, // Ph3 Active Power,
    0x0012, // Ph1 Apparent Power,
    0x0014, // Ph2 Apparent Power,
    0x0016, // Ph3 Apparent Power,
    0x0018, // Ph1 Reactive Power,
    0x001A, // Ph2 Reactive Power,
    0x001C, // Ph3 Reactive Power,
    0x001E, // Ph1 Power Factor,
    0x0020, // Ph2 Power Factor,
    0x0022, // Ph3 Power Factor,
    0x002A, // Avg Voltage Phases to Neutral,
    0x002E, // Avg Current,
    0x0030, // Sum Phase Currents,
    0x0034, // Total System power,
    0x0038, // Total System VoltAmps,
    0x003C, // Total System VAr,
    0x003E, // Total System Power Factor,
    0x0046, // Total System Frequency,
    0x0048, // Total System Import Active Energy (since reset),
    0x004A, // Total System Export Active Energy (since reset),
    0x004C, // Total System Import VArh (since reset),
    0x004E, // Total System Export VArh (since reset),
    0x00C8, // Voltage Ph1 to Ph2,
    0x00CA, // Voltage Ph2 to Ph3,
    0x00CC, // Voltage Ph3 to Ph1,
    0x00CE, // Avg Voltage Phases to Phases,
    0x00E0, // Current Neutral,
    0x0156, // Total kWh (Import + Export),
    0x0158, // Total kVArh (Import + Export),

    // Only in manual for SDM72M-V2:
    0x0180, // Resetable total active energy (kWh),
    0x0182, // Resetable total reactive energy (kVArh),
    0x0184, // Resetable import active energy (kVAh),
    0x0186, // Resetable export active energy (kVAh),
    0x018C, // Net kWh (Import - Export),
    0x0500, // Total import active power,
    0x0502, // Total export active power,
};

uint16_t eastron_sdm72m_holding_registers[] = {
    0x000A, // System type (1 = 1P2W, 3=3P4W)
    0x000C, // Pulse width
    0x000E, // Key Parameter Programming Authorization (0 = no, 1 = yes)
    0x0012, // Parity and Stop bit (0=8N1, 1=8E1, 2=8O1, 3=8N2)
    0x0014, // Modbus slave address (1..247)
    0x0016, // Pulse constant (0=1000imp/kWh, 1=100imp/kWh, 2=10imp/kWh, 3=1imp/kWh)
    0x0018, // Password
    0x001C, // Baud rate (0=2400, 1=4800, 2=9600, 3=19200, 4=38400, 5=1200)
    0x003A, // Automatic scroll display time (0..60)
    0x003C, // Backlight time (0..121 with 0=always on and 121=always off)
    0x0056, // Pulse 1 Energy type (1=import active, 2=total active, 4=export active)
    0xF010, // Write 0x0003 to reset energy info

    // Only in manual for SDM72M-V2:
    0xFC00, // Serial number, 4byte
    0xFC02, // Meter model code, 2byte
    0xFC84, // Firmware version, 2byte
};

#endif // EASTRON_SDM72M_H
