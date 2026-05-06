/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

#include "logging.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/slog2.h>

/**
 * @brief Logging context structure
 */
struct logging_ctx {
    bool initialized;
    unsigned int flags;
    slog2_buffer_t buffer;
    pthread_mutex_t mutex;
};

static struct logging_ctx g_log_ctx = {
    .initialized = false,
    .flags = LOG_FLAG_NONE,
    .buffer = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

/**
 * @brief Get severity name string for display
 */
static const char *get_severity_name(int severity) {
    switch (severity) {
    case SLOG2_SHUTDOWN:
        return "EMERG";
    case SLOG2_CRITICAL:
        return "CRIT";
    case SLOG2_ERROR:
        return "ERROR";
    case SLOG2_WARNING:
        return "WARNING";
    case SLOG2_NOTICE:
        return "NOTICE";
    case SLOG2_INFO:
        return "INFO";
    case SLOG2_DEBUG1:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

int logging_init(const char *ident, unsigned int flags) {
    int ret = EOK;
    slog2_buffer_set_config_t buffer_config;

    if (ident == NULL) {
        return EINVAL;
    }

    ret = pthread_mutex_lock(&g_log_ctx.mutex);
    if (ret != EOK) {
        return ret;
    }

    if (g_log_ctx.initialized) {
        pthread_mutex_unlock(&g_log_ctx.mutex);
        return EALREADY;
    }

    /* Configure slog2 buffer */
    buffer_config.buffer_set_name = ident;
    buffer_config.num_buffers = 1;
    buffer_config.verbosity_level = SLOG2_DEBUG2;
    buffer_config.buffer_config[0].buffer_name = "main";
    buffer_config.buffer_config[0].num_pages = 8;

    /* Register the buffer set */
    ret = slog2_register(&buffer_config, &g_log_ctx.buffer, 0);
    if (ret != EOK) {
        pthread_mutex_unlock(&g_log_ctx.mutex);
        return ret;
    }

    g_log_ctx.flags = flags;
    g_log_ctx.initialized = true;

    pthread_mutex_unlock(&g_log_ctx.mutex);

    return EOK;
}

void logging_shutdown(void) {
    pthread_mutex_lock(&g_log_ctx.mutex);

    if (g_log_ctx.initialized) {
        slog2_reset();
        g_log_ctx.buffer = NULL;
        g_log_ctx.initialized = false;
        g_log_ctx.flags = LOG_FLAG_NONE;
    }

    pthread_mutex_unlock(&g_log_ctx.mutex);
}

void logging_log_impl(int severity, const char *file, int line,
                      const char *fmt, ...) {
    va_list args;
    char message[1024];
    int ret;

    if (!g_log_ctx.initialized || g_log_ctx.buffer == NULL) {
        return;
    }

    va_start(args, fmt);
    ret = vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    if (ret < 0) {
        return;
    }

    /* Log to slog2 with file and line information */
    slog2f(g_log_ctx.buffer, 0, severity, "[%s:%d] %s", file, line, message);

    if (g_log_ctx.flags & LOG_FLAG_STDOUT) {
        const char *basename = strrchr(file, '/');
        if (basename != NULL) {
            basename++;
        } else {
            basename = file;
        }
        fprintf(stdout, "[%s] [%s:%d] %s\n", get_severity_name(severity),
                basename, line, message);
        fflush(stdout);
    }
}
