# WSLg Tracing Examples

This document provides practical examples of how to add and use tracing in WSLg code.

## Basic Function Tracing

### Simple Function with Entry/Exit Tracing

```cpp
void ProcessWindow(int windowId) {
    TRACE_ENTRY();
    LOG_INFO("Processing window %d", windowId);
    
    // ... function logic ...
    
    TRACE_EXIT();
}
```

### Function with Parameter and Return Value Tracing

```cpp
bool ConfigureDisplay(const char* config, int width, int height) {
    TRACE_ENTRY();
    TRACE_VALUE("config", config);
    TRACE_INT("width", width);
    TRACE_INT("height", height);
    
    bool success = ApplyConfiguration(config, width, height);
    LOG_INFO("Configuration result: %s", success ? "success" : "failed");
    
    TRACE_EXIT();
    return success;
}
```

## Error Handling with Tracing

### Error Detection and Logging

```cpp
int InitializeWeston(const char* shellType) {
    TRACE_ENTRY();
    LOG_INFO("Initializing Weston with shell: %s", shellType);
    
    int pid = fork();
    if (pid < 0) {
        LOG_ERROR("Failed to fork Weston process: %s", strerror(errno));
        TRACE_EXIT();
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        TRACE_INT("child_pid", getpid());
        // ... exec weston ...
    } else {
        // Parent process
        TRACE_INT("child_pid", pid);
        LOG_INFO("Weston started with PID %d", pid);
    }
    
    TRACE_EXIT();
    return pid;
}
```

## Conditional Component Tracing

### Component-based Filtering

```cpp
// In ProcessMonitor.cpp
void ProcessMonitor::Monitor() {
    TRACE_ENTRY();
    
    while (m_running) {
        if (TRACE_ENABLED() && TRACE_COMPONENT_ENABLED("ProcessMonitor")) {
            LOG_TRACE("Monitoring processes...");
        }
        
        pid_t pid = waitpid(-1, nullptr, WNOHANG);
        if (pid > 0) {
            LOG_INFO("Process %d exited", pid);
            TRACE_INT("exited_pid", pid);
            
            // Handle process exit
        }
        
        sleep(1);
    }
    
    TRACE_EXIT();
}
```

## Complex Operation Tracing

### Multi-step Operation with Detailed Tracing

```cpp
bool MountSharedResources() {
    TRACE_ENTRY();
    LOG_INFO("Mounting shared resources...");
    
    // Step 1: Mount shared memory
    TRACE_VALUE("mount_point", "/mnt/shared_memory");
    if (mount("shared_memory", "/mnt/shared_memory", "virtiofs", 0, nullptr) != 0) {
        LOG_ERROR("Failed to mount shared memory: %s", strerror(errno));
        return false;
    }
    LOG_INFO("Shared memory mounted successfully");
    
    // Step 2: Mount documentation
    TRACE_VALUE("mount_point", "/mnt/wslg/doc");
    if (mount("/usr/share/doc", "/mnt/wslg/doc", "tmpfs", 0, nullptr) != 0) {
        LOG_ERROR("Failed to mount docs: %s", strerror(errno));
        return false;
    }
    LOG_INFO("Documentation mounted successfully");
    
    TRACE_EXIT();
    return true;
}
```

## Font Monitoring with Tracing

### File System Event Tracing

```cpp
// In FontMonitor.cpp
void FontMonitor::OnFontDirectoryChange(const char* dirPath) {
    TRACE_ENTRY();
    TRACE_VALUE("directory", dirPath);
    
    LOG_INFO("Font directory changed: %s", dirPath);
    
    std::vector<std::string> fonts = ScanFontDirectory(dirPath);
    TRACE_INT("font_count", fonts.size());
    
    for (const auto& font : fonts) {
        if (TRACE_LEVEL_ENABLED(LOG_LEVEL_DEBUG)) {
            LOG_DEBUG("Found font: %s", font.c_str());
        }
    }
    
    NotifyFontChange(fonts);
    TRACE_EXIT();
}
```

## Connection and Data Flow Tracing

### RDP Connection Tracing

```cpp
bool EstablishRDPConnection(const char* address, int port) {
    TRACE_ENTRY();
    TRACE_VALUE("address", address);
    TRACE_INT("port", port);
    
    LOG_INFO("Establishing RDP connection to %s:%d", address, port);
    
    // Connection attempt
    int socket = socket(AF_INET, SOCK_STREAM, 0);
    TRACE_INT("socket_fd", socket);
    
    if (socket < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        TRACE_EXIT();
        return false;
    }
    
    // ... connection logic ...
    
    LOG_INFO("RDP connection established");
    TRACE_EXIT();
    return true;
}
```

## Environment-based Tracing Control

### Checking Trace Configuration at Runtime

```cpp
void InitializeTracing() {
    TRACE_ENTRY();
    
    const char* traceLevel = getenv("WSLG_TRACE_LEVEL");
    const char* traceComponents = getenv("WSLG_TRACE_COMPONENTS");
    
    if (traceLevel) {
        LOG_INFO("Trace level set to: %s", traceLevel);
        TRACE_VALUE("trace_level", traceLevel);
    }
    
    if (traceComponents) {
        LOG_INFO("Tracing components: %s", traceComponents);
        TRACE_VALUE("trace_components", traceComponents);
    }
    
    TRACE_EXIT();
}
```

## Performance-Critical Code Tracing

### Selective Tracing in Loops

```cpp
void ProcessPendingEvents() {
    TRACE_ENTRY();
    
    for (int i = 0; i < eventCount; ++i) {
        // Only log on first, last, and errors to avoid excessive output
        if (i == 0 || i == eventCount - 1) {
            TRACE_INT("event_index", i);
        }
        
        const Event& event = events[i];
        
        try {
            HandleEvent(event);
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing event %d: %s", i, e.what());
            TRACE_INT("error_at_index", i);
        }
    }
    
    TRACE_EXIT();
}
```

## Usage Instructions

### To enable these traces:

```bash
# Trace everything in ProcessMonitor
export WSLG_TRACE_ENABLED=1
export WSLG_TRACE_LEVEL=1
export WSLG_TRACE_COMPONENTS=ProcessMonitor
wsl --system <distro>

# Or trace all components at DEBUG level
export WSLG_TRACE_ENABLED=1
export WSLG_TRACE_LEVEL=2
wsl --system <distro>

# Write traces to file for analysis
export WSLG_TRACE_FILE=/mnt/wslg/detailed_trace.log
wsl --system <distro>
```

### Viewing the output:

```bash
# In the system distro
tail -f /mnt/wslg/stderr.log | grep WSLGd

# Or check the dedicated trace file
cat /mnt/wslg/detailed_trace.log
```

## Best Practices

1. **Use appropriate log levels:**
   - `LOG_TRACE()` for detailed flow and values
   - `LOG_DEBUG()` for diagnostic information
   - `LOG_INFO()` for important events
   - `LOG_ERROR()` for error conditions

2. **Avoid excessive tracing:**
   - Use conditional checks for high-frequency operations
   - Put TRACE calls before/after logical sections, not every line

3. **Include context:**
   - Always include relevant parameters and return values
   - Use meaningful variable names in TRACE_VALUE/TRACE_INT/TRACE_PTR

4. **Performance:**
   - Use TRACE_FUNC_ENTRY/EXIT only when needed
   - In performance-critical sections, check trace level before logging

5. **Error paths:**
   - Always log errors with context
   - Use LogException for exception conditions
