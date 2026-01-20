// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include <cstdlib>
#include <cstring>

// Tracing control via environment variables:
// WSLG_TRACE_LEVEL: 1=TRACE, 2=DEBUG, 3=EXCEPTION, 4=ERROR, 5=INFO
// WSLG_TRACE_COMPONENTS: comma-separated component names to trace (e.g., "ProcessMonitor,FontMonitor")
// WSLG_TRACE_FILE: if set, write traces to this file in addition to stderr

class TraceConfig {
public:
    static TraceConfig& GetInstance() {
        static TraceConfig instance;
        return instance;
    }

    bool IsEnabled() const {
        return m_enabled;
    }

    int GetTraceLevel() const {
        return m_traceLevel;
    }

    bool IsComponentEnabled(const char* component) const {
        if (!m_componentFilter || m_componentFilter[0] == '\0') {
            return true;  // All components enabled if no filter
        }
        return m_enabledComponents.find(component) != m_enabledComponents.end();
    }

    const char* GetTraceFile() const {
        return m_traceFile ? m_traceFile : nullptr;
    }

private:
    TraceConfig() {
        // Check if tracing is enabled
        const char* enableEnv = std::getenv("WSLG_TRACE_ENABLED");
        m_enabled = enableEnv && (std::strcmp(enableEnv, "1") == 0 || std::strcmp(enableEnv, "true") == 0);

        // Get trace level
        const char* levelEnv = std::getenv("WSLG_TRACE_LEVEL");
        m_traceLevel = levelEnv ? std::atoi(levelEnv) : 5;  // Default to INFO level

        // Get component filter
        m_componentFilter = std::getenv("WSLG_TRACE_COMPONENTS");

        // Parse enabled components
        if (m_componentFilter) {
            std::string components = m_componentFilter;
            size_t pos = 0;
            while (pos < components.length()) {
                size_t comma = components.find(',', pos);
                if (comma == std::string::npos) {
                    m_enabledComponents.insert(components.substr(pos));
                    break;
                }
                m_enabledComponents.insert(components.substr(pos, comma - pos));
                pos = comma + 1;
            }
        }

        // Get trace file
        m_traceFile = std::getenv("WSLG_TRACE_FILE");
    }

    bool m_enabled;
    int m_traceLevel;
    const char* m_componentFilter;
    const char* m_traceFile;
    std::set<std::string> m_enabledComponents;
};

// Conditional tracing macros
#define TRACE_ENABLED() (TraceConfig::GetInstance().IsEnabled())
#define TRACE_COMPONENT_ENABLED(comp) (TraceConfig::GetInstance().IsComponentEnabled(comp))
#define TRACE_LEVEL_ENABLED(level) (TraceConfig::GetInstance().GetTraceLevel() <= (level))

#ifdef ENABLE_DETAILED_TRACING
    #define TRACE_FUNC_ENTRY() TRACE_ENTRY()
    #define TRACE_FUNC_EXIT() TRACE_EXIT()
    #define TRACE_VALUE(name, value) LOG_TRACE("%s = %s", #name, value)
    #define TRACE_INT(name, value) LOG_TRACE("%s = %d", #name, value)
    #define TRACE_PTR(name, ptr) LOG_TRACE("%s = %p", #name, ptr)
#else
    #define TRACE_FUNC_ENTRY()
    #define TRACE_FUNC_EXIT()
    #define TRACE_VALUE(name, value)
    #define TRACE_INT(name, value)
    #define TRACE_PTR(name, ptr)
#endif
