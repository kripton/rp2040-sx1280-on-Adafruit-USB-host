#include "webserver.h"

#include <string>

#include <pico/unique_id.h>
#include <pico/bootrom.h>
#include <hardware/flash.h>
#include <hardware/watchdog.h>

#include "json/json.h"

#include "log.h"

extern char __StackLimit; /* Set by linker.  */

// Borrowed from main.cpp
extern Json::Value storage;

static const tCGI cgi_handlers[] = {
    {"api/reset/boot",
    cgi_api_reset_boot},
    {"/api/reset/system",
    cgi_api_reset_system},
};

// This array doesn't need elements since we are using LWIP_HTTPD_SSI_RAW
static const char *ssiTags[] = {};

void WebServer::init()
{
    // Initialize lwip, dhcpd and httpd
    // TinyUSB already needs to be initialized at this point
    httpd_init();
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
    http_set_ssi_handler(ssi_handler, ssiTags, LWIP_ARRAYSIZE(ssiTags));
}

void WebServer::cyclicTask()
{
    service_traffic();

    sys_check_timeouts();
}

void WebServer::ipToString(uint32_t ip, char *ipString)
{
    sprintf(ipString, "%ld.%ld.%ld.%ld", (ip & 0xff), ((ip >> 8) & 0xff), ((ip >> 16) & 0xff), ((ip >> 24) & 0xff));
}

static const char *cgi_api_reset_boot(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    reset_usb_boot(0, 0);
    return "/empty.html";
}

static const char *cgi_api_reset_system(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    watchdog_reboot(0, 0, 100);
    return "/empty.html";
}

static u16_t ssi_handler(const char *ssi_tag_name, char *pcInsert, int iInsertLen)
{
    return WebServer::ssi_handler(ssi_tag_name, pcInsert, iInsertLen);
}

u16_t WebServer::ssi_handler(const char *ssi_tag_name, char *pcInsert, int iInsertLen)
{
    // Called once per Tag, no matter which file has been requested

    std::string tagName(ssi_tag_name);
    Json::Value output;
    Json::StreamWriterBuilder wbuilder;
    std::string output_string;

    wbuilder["indentation"] = "";

    if (tagName == "OverviewGet")
    {
        char status[4];
        pico_unique_board_id_t board_id;
        char unique_id_string[25];

        char ip[20]; // Can hold a mac in string format as well
        char scratchbuf[42]; // longest: UUID

        pico_get_unique_board_id(&board_id);
        snprintf(unique_id_string, 24, "%02x%02x%02x%02x%02x%02x%02x%02x",
                 board_id.id[0],
                 board_id.id[1],
                 board_id.id[2],
                 board_id.id[3],
                 board_id.id[4],
                 board_id.id[5],
                 board_id.id[6],
                 board_id.id[7]);

        output["debug"]["flash"]["totalSize"] = PICO_FLASH_SIZE_BYTES;
        output["debug"]["flash"]["sectorSize"] = FLASH_SECTOR_SIZE;
        output["debug"]["flash"]["blockSize"] = FLASH_BLOCK_SIZE;

        output["debug"]["toolchain"]["cmake_version"] = CMAKE_VERSION;
#if defined(CI_BUILD)
        output["debug"]["toolchain"]["ci_build"] = true;
#else
        output["debug"]["toolchain"]["ci_build"] = false;
#endif
        output["debug"]["toolchain"]["pico_sdk_version"] = PICO_SDK_VERSION_STRING;
        output["debug"]["toolchain"]["PICO_BOARD"] = PICO_BOARD;
        output["debug"]["toolchain"]["lwip_version"] = LWIP_VERSION_STRING;
        snprintf(scratchbuf, 11, "%d.%d.%d", TUSB_VERSION_MAJOR, TUSB_VERSION_MINOR, TUSB_VERSION_REVISION);
        output["debug"]["toolchain"]["tinyusb_version"] = scratchbuf;
#if defined(__GNUC__)
# if defined(__GNUC_PATCHLEVEL__)
#  define __GNUC_VERSION__ (__GNUC__ * 10000 \
                            + __GNUC_MINOR__ * 100 \
                            + __GNUC_PATCHLEVEL__)
# else
#  define __GNUC_VERSION__ (__GNUC__ * 10000 \
                            + __GNUC_MINOR__ * 100)
# endif
        output["debug"]["toolchain"]["compiler_name"] = "GNU gcc";
        output["debug"]["toolchain"]["compiler_version"] = __GNUC_VERSION__;
#endif
#if defined(__clang_version__)
        output["debug"]["toolchain"]["compiler_name"] = "LLVM clang";
        output["debug"]["toolchain"]["compiler_version"] = __clang_version__;
#endif
#if defined (__cplusplus)
        output["debug"]["toolchain"]["cplusplus_standard"] = (int64_t)__cplusplus;
#endif
        output["debug"]["toolchain"]["stackLimit"] = __StackLimit;

        output["version"] = VERSION;
        output["serial"] = unique_id_string;

        struct netif* iface = netif_list;
        while (iface != nullptr) {
            sprintf(scratchbuf, "%c%c", iface->name[0], iface->name[1]);
            WebServer::ipToString(iface->ip_addr.addr, ip);
            output["net"][scratchbuf]["ip"] = ip;
            WebServer::ipToString(iface->netmask.addr, ip);
            output["net"][scratchbuf]["netmask"] = ip;
            WebServer::ipToString(iface->gw.addr, ip);
            output["net"][scratchbuf]["gw"] = ip;
#if LWIP_NETIF_HOSTNAME
            if (iface->hostname) {
                output["net"][scratchbuf]["hostname"] = iface->hostname;
            }
#endif

            if ((iface->name[0] == 'u') && (iface->name[1] == '0')) {
                //WebServer::ipToString(boardConfig.activeConfig->hostIp, ip);
                //output["net"][scratchbuf]["hostip"] = ip;
            } else if ((iface->name[0] == 'e') && (iface->name[1] == '0')) {
            }

            iface = iface->next;
        }

        output_string = Json::writeString(wbuilder, output);
        return snprintf(pcInsert, iInsertLen, "%s", output_string.c_str());
    }
    if (tagName == "Meters")
    {
        storage["tsNow"] = (Json::UInt)(time_us_64() / 1000);

        output_string = Json::writeString(wbuilder, storage);
        return snprintf(pcInsert, iInsertLen, "%s", output_string.c_str());
    }
    else if (tagName == "LogGet")
    {
        // Don't use jsoncpp here for performance reasons, write directly to pcInsert

        uint32_t offset = 0;

        offset += sprintf(pcInsert + offset, "{\"log\":[");

        offset += Log::getLogBuffer(pcInsert + offset, iInsertLen - 40);
        size_t remaining = Log::getLogBufferNumEntries();
        offset += sprintf(pcInsert + offset, "], \"remaining\": %d}", remaining);

        return offset;
    }
    else
    {
        return HTTPD_SSI_TAG_UNKNOWN;
    }
}

void WebServer::paramsToMap(int iNumParams, char *pcParam[], char *pcValue[], std::map<std::string, std::string> *params)
{
    for (int i = 0; i < iNumParams; i++)
    {
        if ((pcParam[i] == nullptr) || (pcValue[i] == nullptr))
        {
            continue;
        }
        (*params)[std::string(pcParam[i])] = std::string(pcValue[i]);
    }
}

// From: https://stackoverflow.com/a/4823686
std::string WebServer::urlDecode(std::string &SRC)
{
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < SRC.length(); i++)
    {
        if (int(SRC[i]) == '%')
        {
            sscanf(SRC.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
        else
        {
            ret += SRC[i];
        }
    }
    return (ret);
}
