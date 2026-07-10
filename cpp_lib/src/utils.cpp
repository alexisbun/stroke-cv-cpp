#include "utils.h"
#include <spdlog/spdlog.h>
#include <android/log.h>
#include <spdlog/sinks/android_sink.h>

void utils::init_native_logging() {
    try {
        auto android_logger = spdlog::android_logger_mt("stroke_cv_logger", "StrokeCV_Native");
        spdlog::set_default_logger(android_logger);
        spdlog::set_pattern("%v");
        spdlog::set_level(spdlog::level::debug);
        
        spdlog::info("spdlog initialized successfully via init_native_logging!");
    } catch (const spdlog::spdlog_ex& ex) {
        __android_log_print(ANDROID_LOG_ERROR, "StrokeCV_Native", "Failed to initialize spdlog: %s", ex.what());
    }
} 