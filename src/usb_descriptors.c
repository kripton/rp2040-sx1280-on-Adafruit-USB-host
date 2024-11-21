/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

#include <pico/stdlib.h>
#include <pico/unique_id.h>

#include <stdlib.h>   // #malloc

#include "version.h"

#include "log.h"

// Please get something else :)
#define DEFAULT_VID 0x1209
#define DEFAULT_PID 0x000B

// String descriptor indices
enum
{
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_CDC_ACM_IFNAME,
  STRID_CDC_NCM_IFNAME,
  STRID_MAC,
  STRID_VENDOR,
};

// Available interfaces
// ATTENTION: The order here seems to be VERY important!
// HID not in first place => OLA won't work
enum {
    ITF_NUM_CDC_ACM_CMD,
    ITF_NUM_CDC_ACM_DATA,
    ITF_NUM_CDC_NCM_CMD,
    ITF_NUM_CDC_NCM_DATA,
    ITF_NUM_VENDOR,
    ITF_NUM_TOTAL
};

// Available configurations
// Since NCM works on Linux, macOS and Windows hosts, we just have one config
enum
{
  CONFIG_ID_NCM   = 0,
  CONFIG_ID_COUNT
};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0210,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = DEFAULT_VID, // Possibly overwritten by function below
    .idProduct          = DEFAULT_PID, // Possibly overwritten by function below
    .bcdDevice          = VERSION_BCD,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = CONFIG_ID_COUNT
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
// If none matches, the DEFAULT one will be used
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define  CONFIG_NCM_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_CDC_NCM_DESC_LEN + TUD_VENDOR_DESC_LEN)

#define EPNUM_CDC_ACM_CMD        0x83
#define EPNUM_CDC_ACM_OUT        0x04
#define EPNUM_CDC_ACM_IN         0x84
#define USBD_CDC_CMD_MAX_SIZE       8
#define USBD_CDC_IN_OUT_MAX_SIZE   64

#define EPNUM_CDC_NCM_CMD        0x85
#define EPNUM_CDC_NCM_OUT        0x06
#define EPNUM_CDC_NCM_IN         0x86

#define EPNUM_VENDOR_OUT         0x07
#define EPNUM_VENDOR_IN          0x87

uint8_t const ncm_configuration[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(CONFIG_ID_NCM+1, ITF_NUM_TOTAL, 0, CONFIG_NCM_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_ACM_CMD, STRID_CDC_ACM_IFNAME, EPNUM_CDC_ACM_CMD,
        USBD_CDC_CMD_MAX_SIZE, EPNUM_CDC_ACM_OUT, EPNUM_CDC_ACM_IN,
        USBD_CDC_IN_OUT_MAX_SIZE),

    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC_NCM_CMD, STRID_CDC_NCM_IFNAME, STRID_MAC, EPNUM_CDC_NCM_CMD, 64, EPNUM_CDC_NCM_OUT, EPNUM_CDC_NCM_IN, CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),

    // Interface number, string index, EP Out & IN address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, STRID_VENDOR, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN, 64),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return ncm_configuration;
}

//--------------------------------------------------------------------+
// BOS Descriptor
//--------------------------------------------------------------------+

/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.
GUID is freshly generated and should be OK to use.
https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
*/

enum
{
  VENDOR_REQUEST_WEBUSB = 1,
  VENDOR_REQUEST_MICROSOFT = 2
};

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)



// MS_OS_20_TEMPLATE definitions:

#define TUD_MSOS20_HEADER_LEN 10
#define TUD_MSOS20_CONFIG_SUBSET_LEN 8
#define TUD_MSOS20_FUNCTION_SUBSET_LEN 8
#define TUD_MSOS20_COMPATIBLE_ID_LEN 20
#define TUD_MSOS20_REG_PROPERTY_BASE_LEN 10

#define TUD_MSOS20_COMPATIBLE_ID_NONE 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define TUD_MSOS20_COMPATIBLE_ID_WINNCM 'W', 'I', 'N', 'N', 'C', 'M', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define TUD_MSOS20_COMPATIBLE_ID_WINUSB 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

// Windows version, Descriptor total length
#define TUD_MSOS20_HEADER(_winVer, _totalLength) \
  /* Set header: length, type, windows version, total length */\
  U16_TO_U8S_LE(TUD_MSOS20_HEADER_LEN), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(_winVer), U16_TO_U8S_LE(_totalLength)

// Configuration index, configuration total length
#define TUD_MSOS20_CONFIG_SUBSET(_idxConfig, _configTotalLength) \
  /* Configuration subset header: length, type, configuration index, reserved, configuration total length */\
  U16_TO_U8S_LE(TUD_MSOS20_CONFIG_SUBSET_LEN), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), _idxConfig, 0, U16_TO_U8S_LE(_configTotalLength)

// Index of first interface, Function subset total length
#define TUD_MSOS20_FUNCTION_SUBSET(_idxInterface, _functionTotalLength) \
  /* Function Subset header: length, type, first interface, reserved, subset length */\
  U16_TO_U8S_LE(TUD_MSOS20_FUNCTION_SUBSET_LEN), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), _idxInterface, 0, U16_TO_U8S_LE(_functionTotalLength)

// Compatible ID identifier
#define TUD_MSOS20_COMPATIBLE_ID(_compatibleId) \
  /* Compatible ID descriptor: length, type, compatible ID, sub compatible ID */\
  U16_TO_U8S_LE(TUD_MSOS20_COMPATIBLE_ID_LEN), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), _compatibleId

// Registry property descriptor
#define TUD_MSOS20_REGISTRY_PROPERTY(_dataType, _nameLength, _name, _dataLength, _data) \
  /* Registry property descriptor: descriptorLength, descriptorType, dataType, nameLength, name, dataLength, data */\
  U16_TO_U8S_LE(TUD_MSOS20_REG_PROPERTY_BASE_LEN + _nameLength + _dataLength), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),\
  U16_TO_U8S_LE(_dataType), U16_TO_U8S_LE(_nameLength), _name,\
  U16_TO_U8S_LE(_dataLength), _data

#define P99_PROTECT(...) __VA_ARGS__







#define LEN_REG (TUD_MSOS20_REG_PROPERTY_BASE_LEN + 42 + 80)
#define LEN_FUNCTION_0 (TUD_MSOS20_FUNCTION_SUBSET_LEN + TUD_MSOS20_COMPATIBLE_ID_LEN)
#define LEN_FUNCTION_1 (TUD_MSOS20_FUNCTION_SUBSET_LEN + TUD_MSOS20_COMPATIBLE_ID_LEN + LEN_REG)
#define LEN_FUNCTION_2 (TUD_MSOS20_FUNCTION_SUBSET_LEN + TUD_MSOS20_COMPATIBLE_ID_LEN + LEN_REG)
//#define LEN_CONFIG (TUD_MSOS20_CONFIG_SUBSET_LEN + LEN_FUNCTION_0 + LEN_FUNCTION_1 + LEN_FUNCTION_2)
#define LEN_CONFIG (LEN_FUNCTION_0 + LEN_FUNCTION_1 + LEN_FUNCTION_2)
#define LEN_TOTAL (LEN_CONFIG + TUD_MSOS20_HEADER_LEN)

uint8_t const desc_ms_os_20[] =
{
  // Microsoft OS 2.0 Descriptor HEADER:
  // Windows version
  TUD_MSOS20_HEADER(0x06030000, LEN_TOTAL),

    // Microsoft OS 2.0 Descriptor Configuration subset header:
    // Configuration index, configuration total length
    //TUD_MSOS20_CONFIG_SUBSET(0, LEN_CONFIG),

      // FUNCTION 0: CDC ACM ()
      // Index of first interface, Function subset total length
      TUD_MSOS20_FUNCTION_SUBSET(ITF_NUM_CDC_ACM_CMD, LEN_FUNCTION_0),

      // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
      TUD_MSOS20_COMPATIBLE_ID(TUD_MSOS20_COMPATIBLE_ID_NONE)
      // END FUNCTION 0

      ,

      // FUNCTION 1: CDC NCM
      // Index of first interface, Function subset total length
      TUD_MSOS20_FUNCTION_SUBSET(ITF_NUM_CDC_NCM_CMD, LEN_FUNCTION_1),

      // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
      TUD_MSOS20_COMPATIBLE_ID(TUD_MSOS20_COMPATIBLE_ID_WINNCM),

      // MS OS 2.0 Registry property descriptor: dataType, nameLength, name, dataLength, data
      TUD_MSOS20_REGISTRY_PROPERTY(7, 42, P99_PROTECT(
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00),
        80, P99_PROTECT(
        '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
        '0', 0x00, 'D', 0x00, '6', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
        '8', 0x00, 'B', 0x00, '3', 0x00, 'D', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
        '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
        )
      )
      // END FUNCTION 1

      ,

      // FUNCTION 2: WEB USB (Vendor Interface) via "WinUSB Driver"
      // Index of first interface, Function subset total length
      TUD_MSOS20_FUNCTION_SUBSET(ITF_NUM_VENDOR, LEN_FUNCTION_2),

      // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
      TUD_MSOS20_COMPATIBLE_ID(TUD_MSOS20_COMPATIBLE_ID_WINUSB),

      // MS OS 2.0 Registry property descriptor: dataType, nameLength, name, dataLength, data
      TUD_MSOS20_REGISTRY_PROPERTY(7, 42, P99_PROTECT(
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00),
        80, P99_PROTECT(
        '{', 0x00, '3', 0x00, '7', 0x00, '5', 0x00, 'C', 0x00, '2', 0x00, '4', 0x00, '8', 0x00, '9', 0x00, '-', 0x00,
        '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '3', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
        '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '4', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
        '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'A', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
        )
      )
};

//TU_VERIFY_STATIC(sizeof(desc_ms_os_20)<0xffff,"Too large!");
//TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");


// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] =
{
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

  // Vendor Code, iLandingPage
  TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

  // Microsoft OS 2.0 descriptor
  TUD_BOS_MS_OS_20_DESCRIPTOR(LEN_TOTAL, VENDOR_REQUEST_MICROSOFT)
};

uint8_t const * tud_descriptor_bos_cb(void)
{
  return desc_bos;
}


//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  tusb_desc_webusb_url_t* desc_url;

  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) {
    return true;
  }

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_WEBUSB:
          LOG("VENDOR_REQUEST_WEBUSB");
          // match vendor request in BOS descriptor
          // Get landing page url

          pico_unique_board_id_t board_id;
          pico_get_unique_board_id(&board_id);

          desc_url = malloc(90);
          uint32_t ip = 0x0100fea9UL;  // 169.254.0.1
          ip = (ip & 0xff00ffff) | ((uint32_t)board_id.id[6] << 16);
          snprintf(desc_url->url, 85, "%ld.%ld.%ld.%ld", (ip & 0xff), ((ip >> 8) & 0xff), ((ip >> 16) & 0xff), ((ip >> 24) & 0xff));
          desc_url->bLength = 3 + strlen(desc_url->url);
          desc_url->bDescriptorType = 3; //Web USB URL type
          desc_url->bScheme = 0; // 0: http, 1: https
          bool retVal = tud_control_xfer(rhport, request, (void*)desc_url, desc_url->bLength);
          free(desc_url);
          return retVal;

        case VENDOR_REQUEST_MICROSOFT:
          LOG("VENDOR_REQUEST_MICROSOFT");
          if ( request->wIndex == 7 ) {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            LOG("wIndex == 7. DESCRIPTOR LENGTH: %d", total_len);

            return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
          } else {
            return false;
          }

        default: break;
      }
    break;

    default: break;
  }

  // stall unknown request
  return false;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
    [STRID_LANGID]         = (const char[]) {0x09, 0x04},            // 0: is supported language is English (0x0409)
    [STRID_MANUFACTURER]   = "MANUFACTURER",                         // 1: Manufacturer
    [STRID_PRODUCT]        = "PRODUCT",                              // 2: Product, fallback here, it's dynamically created in tud_descriptor_string_cb
    [STRID_SERIAL]         = "RP2040_0123456789ABCDEF",              // 3: Serial, fallback here, it's dynamically created in tud_descriptor_string_cb
    [STRID_CDC_ACM_IFNAME] = "Debugging Console",                    // 4: CDC ACM interface name
    [STRID_CDC_NCM_IFNAME] = "Network Interface",                    // 5: CDC NCM interface name
    [STRID_MAC]            = "000000000000",                         // 6: MAC address is handled in tud_descriptor_string_cb
    [STRID_VENDOR]         = "WebUSB"                                // 7: Vendor Interface
};

static uint16_t _desc_str[128];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    char *str = NULL;
    uint8_t chr_count = 0;

    LOG("USB STRING REQUESTED! INDEX: 0x%02x", index);

    // Serial number has been requested, construct it from the unique board id
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    if (index == STRID_SERIAL) {
        char serial[26];
        str = serial;

        snprintf(serial, 24, "%02x%02x%02x%02x%02x%02x%02x%02x",
            board_id.id[0],
            board_id.id[1],
            board_id.id[2],
            board_id.id[3],
            board_id.id[4],
            board_id.id[5],
            board_id.id[6],
            board_id.id[7]
        );
    } else if (index == STRID_PRODUCT) {
      // Network interface name has been requested. Get our IP there
      char product[64];
      uint32_t ip = 0x0100fea9UL;  // 169.254.0.1
      ip = (ip & 0xff00ffff) | ((uint32_t)board_id.id[6] << 16);
      str = product;
      snprintf(product, 64, "%s %d.%d.%d.%d",
        "PRODUCT, IP",
        (uint8_t)(ip& 0xff),
        (uint8_t)((ip >> 8) & 0xff),
        (uint8_t)((ip >> 16) & 0xff),
        (uint8_t)((ip >> 24) & 0xff)
      );
    }

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == STRID_MAC) {
        // Convert MAC address directly into UTF-16
        for (unsigned i=0; i<sizeof(tud_network_mac_address); i++)
        {
          _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xf];
          _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 0) & 0xf];
        }
    } else {
        // Convert ASCII string into UTF-16

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

        if (str == NULL) {
            str = (char*)string_desc_arr[index];
        }

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 63) chr_count = 63;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
