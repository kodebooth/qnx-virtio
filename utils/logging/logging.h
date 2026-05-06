/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file logging.h
 * @brief Thread-safe logging library with slog2 support
 *
 * This module provides a logging interface that wraps slog2 with additional
 * features including optional stdout output and automatic file/line tracking.
 * The logging functions are implemented as macros to capture source location.
 */

#ifndef QNX_VIRTIO_UTILS_LOGGING_LOGGING_H_INCLUDED
#define QNX_VIRTIO_UTILS_LOGGING_LOGGING_H_INCLUDED

#include <sys/slog2.h>

/**
 * @brief Logging configuration flags
 */
enum log_flags {
  LOG_FLAG_NONE = 0,
  LOG_FLAG_STDOUT = (1 << 0), /**< Also print logs to stdout */
};

/**
 * @brief Initialize the logging system
 *
 * Initializes the logging system with slog2. This must be called before
 * any logging operations.
 *
 * @param[in] ident Logger identifier (typically program name)
 * @param[in] flags Logging configuration flags (combination of log_flags)
 *
 * @return EOK on success, or an error code on failure
 */
int logging_init(const char *ident, unsigned int flags);

/**
 * @brief Shutdown the logging system
 *
 * Closes the slog2 connection and cleans up logging resources.
 */
void logging_shutdown(void);

/**
 * @brief Internal logging function (do not call directly)
 *
 * This function is used by the logging macros to implement the actual
 * logging functionality. Use the LOG_* macros instead of calling this
 * directly.
 *
 * @param[in] severity slog2 severity level
 * @param[in] file Source file name
 * @param[in] line Source line number
 * @param[in] fmt Format string (printf-style)
 * @param[in] ... Format arguments
 */
void logging_log_impl(int severity, const char *file, int line, const char *fmt,
                      ...) __attribute__((format(printf, 4, 5)));

/**
 * @brief Log an emergency message (system is unusable)
 */
#define log_emerg(fmt, ...)                                                    \
  logging_log_impl(SLOG2_SHUTDOWN, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an alert message (action must be taken immediately)
 */
#define log_alert(fmt, ...)                                                    \
  logging_log_impl(SLOG2_CRITICAL, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a critical message (critical conditions)
 */
#define log_crit(fmt, ...)                                                     \
  logging_log_impl(SLOG2_CRITICAL, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an error message
 */
#define log_err(fmt, ...)                                                      \
  logging_log_impl(SLOG2_ERROR, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a warning message
 */
#define log_warn(fmt, ...)                                                     \
  logging_log_impl(SLOG2_WARNING, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a notice message (normal but significant condition)
 */
#define log_notice(fmt, ...)                                                   \
  logging_log_impl(SLOG2_NOTICE, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an informational message
 */
#define log_info(fmt, ...)                                                     \
  logging_log_impl(SLOG2_INFO, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a debug message
 */
#define log_debug(fmt, ...)                                                    \
  logging_log_impl(SLOG2_DEBUG1, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

#endif
