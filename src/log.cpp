#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include <tusb.h>

uint32_t Log::logLineCount;
queue_t Log::logQueue;
mutex_t Log::logLock;
char Log::logLine[512];
bool Log::stdioReady;

void Log::init() {
    mutex_init(&logLock);
    queue_init(&logQueue, 512, LOG_BUFFER_SIZE);
    Log::logLineCount = 0;
    Log::stdioReady = false;

    LOG("--- LOGGER INITIALISED --- LOG STARTS HERE ---");
}

void Log::dlog(char* file, uint32_t line, char* text) {
    // Input sanitation so that one cannot make it invalid JSON
    // TODO: RegEx is damn slow and memory-hungry. Any idea for something leaner?
    //std::string bufSanitized = std::string(text);
    //bufSanitized = std::regex_replace(bufSanitized, std::regex("\""), "\\\"");
    //bufSanitized = std::regex_replace(bufSanitized, std::regex("\n"), "\\n");

    std::string fname = std::string(file);
    auto const pos = fname.find_last_of('/');
    fname = fname.substr(pos + 1);
    uint32_t ms = (uint32_t)(time_us_64() / 1000);

    // If ACM console IS connected, just print it
    // If ACM console is not connected, append to log buffer (of course size-limitig it)
    if (Log::stdioReady && tud_cdc_connected()) {
        printf("{\"tp\": \"log\", \"count\": %ld, \"ms\": %ld, \"core\": %u, \"file\": \"%s\", \"line\": %ld, \"text\": \"%s\"}\n", logLineCount, ms, get_core_num(), fname.c_str(), line, text);
        tud_cdc_write_flush();
    } else {
        mutex_enter_blocking(&logLock);
        if (queue_is_full(&logQueue)) {
            queue_remove_blocking(&logQueue, logLine);
        }
        snprintf(logLine, 512, "{\"tp\": \"log\", \"count\": %ld, \"ms\": %ld, \"core\": %u, \"file\": \"%s\", \"line\": %ld, \"text\": \"%s\"}\n", logLineCount, ms, get_core_num(), fname.c_str(), line, text);
        queue_add_blocking(&logQueue, logLine);
        mutex_exit(&logLock);
    }

    Log::logLineCount++;
}

void Log::dloghex(char* file, uint32_t line, uint8_t size, uint8_t* data) {
    char lText[500];
    uint16_t spent = 0;
    uint16_t inCnt = 0;
    while ((spent < 500) && (inCnt < size)) {
        spent += snprintf(lText + spent, (500 - spent), "%02x ", data[inCnt++]);
    }
    dlog(file, line, lText);
}

size_t Log::getLogBufferNumEntries() {
    return queue_get_level(&logQueue);
}

size_t Log::getLogBuffer(char* buffer, size_t size) {
    size_t offset = 0;

    if (buffer == 0) {
        return 0;
    }

    // Avoid an underflow later when the loop is not run but we
    // subtract from the offset anyways
    if (size < 12) {
        return offset;
    }

    while(!queue_is_empty(&logQueue))
    {
        // Make sure we have enough space left in the buffer
        if ((size - offset) < 510) {
            break;
        }
        queue_remove_blocking(&logQueue, logLine);
        offset += snprintf(buffer + offset, size - offset, "%s,\n", logLine);
    }

    // Remove the last comma and line break
    if (offset > 5) {
        offset -= 2;
    }
    mutex_exit(&logLock);

    return offset;
}

void Log::clearLogBuffer() {
    mutex_enter_blocking(&logLock);
    while (!queue_is_empty(&logQueue))
    {
        queue_remove_blocking(&logQueue, logLine);
    }
    mutex_exit(&logLock);
}

// C helper functions
void dlog(char* file, uint32_t line, char* text, ...) {
    va_list args;
    char buf[1024];

    va_start(args, text);
    vsnprintf(buf, 1024, text, args);
    va_end(args);

    Log::dlog(file, line, buf);
}

void dloghex(char* file, uint32_t line, uint8_t size, uint8_t* data) {
    Log::dloghex(file, line, size, data);
}
