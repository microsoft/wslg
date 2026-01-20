# WSLg Tracing Guide

This document describes the tracing infrastructure available in WSLg for debugging and performance analysis.

## Overview

WSLg includes comprehensive tracing support via:
1. Log levels (TRACE, DEBUG, ERROR, INFO, EXCEPTION)
2. Component-based filtering
3. Environment variable configuration
4. Runtime call tracing

## Log Levels

The following log levels are defined (lower numbers = more verbose):

| Level | Macro | Use Case |
|-------|-------|----------|
| 1 | `LOG_TRACE()` | Function entry/exit, variable values, detailed flow |
| 2 | `LOG_DEBUG()` | Debugging information, intermediate values |
| 3 | `LOG_EXCEPTION()` | Exception and error details |
| 4 | `LOG_ERROR()` | Error conditions |
| 5 | `LOG_INFO()` | General informational messages |

## Macros

### Basic Logging

```c
LOG_TRACE(fmt, ...)        // Trace level logging
LOG_DEBUG(fmt, ...)        // Debug level logging
LOG_INFO(fmt, ...)         // Info level logging
LOG_ERROR(fmt, ...)        // Error level logging
LogException(msg, desc)    // Exception logging
```

### Tracing Helpers

```c
TRACE_ENTRY()              // Log function entry point
TRACE_EXIT()               // Log function exit point
TRACE_CALL(func)           // Log when calling a function
```

### Detailed Tracing (when ENABLE_DETAILED_TRACING is defined)

```c
TRACE_FUNC_ENTRY()         // Conditional function entry logging
TRACE_FUNC_EXIT()          // Conditional function exit logging
TRACE_VALUE(name, value)   // Trace string value: name = value
TRACE_INT(name, value)     // Trace integer value: name = 123
TRACE_PTR(name, ptr)       // Trace pointer value: name = 0x12345678
```

## Environment Variables

Control tracing behavior via environment variables:

### WSLG_TRACE_ENABLED
Enable/disable the tracing system.
```bash
export WSLG_TRACE_ENABLED=1
export WSLG_TRACE_ENABLED=true
```

### WSLG_TRACE_LEVEL
Set the minimum trace level to display (1-5, lower = more verbose).
```bash
export WSLG_TRACE_LEVEL=1    # Show everything (TRACE and above)
export WSLG_TRACE_LEVEL=2    # DEBUG level and above
export WSLG_TRACE_LEVEL=4    # ERROR level and above
export WSLG_TRACE_LEVEL=5    # INFO level only (default)
```

### WSLG_TRACE_COMPONENTS
Filter tracing to specific components (comma-separated).
```bash
export WSLG_TRACE_COMPONENTS=ProcessMonitor,FontMonitor
export WSLG_TRACE_COMPONENTS=*    # Trace all components
```

### WSLG_TRACE_FILE
Write traces to a file in addition to stderr.
```bash
export WSLG_TRACE_FILE=/mnt/wslg/traces.log
```

## Usage Examples

### Example 1: Full Trace of ProcessMonitor
```bash
export WSLG_TRACE_ENABLED=1
export WSLG_TRACE_LEVEL=1
export WSLG_TRACE_COMPONENTS=ProcessMonitor
wsl --system <distro>
```

### Example 2: Debug-level traces with file output
```bash
export WSLG_TRACE_ENABLED=1
export WSLG_TRACE_LEVEL=2
export WSLG_TRACE_FILE=/mnt/wslg/debug.log
wsl --system <distro>
```

### Example 3: Error and Exception tracing only
```bash
export WSLG_TRACE_LEVEL=3
wsl --system <distro>
```

## Integration with Code

### Adding Tracing to Functions

```c
void MyFunction(int param) {
    TRACE_ENTRY();
    TRACE_INT("param", param);
    
    // Function logic
    int result = DoSomething();
    TRACE_INT("result", result);
    
    TRACE_EXIT();
}
```

### Conditional Tracing

```c
if (TRACE_ENABLED()) {
    if (TRACE_COMPONENT_ENABLED("ProcessMonitor")) {
        LOG_TRACE("Detailed process information...");
    }
}
```

### Level-based Tracing

```c
if (TRACE_LEVEL_ENABLED(LOG_LEVEL_DEBUG)) {
    LOG_DEBUG("Verbose debug information");
}
```

## Log Output Format

Logs are printed to stderr with the format:
```
[HH:MM:SS.mmm] <LEVEL> WSLGd: function:line: message
```

Example:
```
[14:23:45.123] <1> WSLGd: ProcessMonitor::Start:42: >>> Entering
[14:23:45.124] <2> WSLGd: ProcessMonitor::Start:45: param = 5
[14:23:45.125] <1> WSLGd: ProcessMonitor::Start:50: <<< Exiting
```

## Viewing Traces

### In the System Distro
```bash
wsl --system <distro>
ps aux | grep -E 'weston|wslgd|pulse'
cat /mnt/wslg/stderr.log
```

### From Windows
Traces written to the WSLG_TRACE_FILE can be viewed with any text editor:
```powershell
Get-Content "\\wsl.localhost\<distro>\mnt\wslg\traces.log" -Tail 100
```

## Performance Considerations

- Tracing has minimal overhead when disabled
- TRACE_FUNC_ENTRY() and similar detailed macros only compile when ENABLE_DETAILED_TRACING is defined
- For production builds, use LOG_LEVEL_INFO or higher
- TRACE level logging should only be used during development/debugging

## Related Files

- `WSLGd/common.h` - Core logging macros and log levels
- `WSLGd/trace.h` - Advanced tracing configuration and helpers
- `WSLGd/main.cpp` - Environment variable initialization
