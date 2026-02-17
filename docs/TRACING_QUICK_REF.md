# WSLg Tracing Quick Reference

## Enable Tracing

```bash
# Full trace (most verbose)
export WSLG_TRACE_ENABLED=1 && export WSLG_TRACE_LEVEL=1

# Debug level
export WSLG_TRACE_LEVEL=2

# Info level (default)
export WSLG_TRACE_LEVEL=5

# Trace specific component
export WSLG_TRACE_COMPONENTS=ProcessMonitor

# Write to file
export WSLG_TRACE_FILE=/mnt/wslg/trace.log
```

## Logging Macros

| Macro | Level | Usage |
|-------|-------|-------|
| `LOG_TRACE(fmt, ...)` | 1 | Detailed flow tracing |
| `LOG_DEBUG(fmt, ...)` | 2 | Debug information |
| `LOG_ERROR(fmt, ...)` | 4 | Errors |
| `LOG_INFO(fmt, ...)` | 5 | Information |

## Tracing Helpers

```cpp
TRACE_ENTRY();              // Log function entry
TRACE_EXIT();               // Log function exit
TRACE_CALL(function_name);  // Log function call
TRACE_VALUE(name, str);     // Log string value (requires ENABLE_DETAILED_TRACING)
TRACE_INT(name, val);       // Log int value
TRACE_PTR(name, ptr);       // Log pointer value
```

## Example: Adding Tracing to a Function

```cpp
void MyFunction(const char* param) {
    TRACE_ENTRY();
    TRACE_VALUE("param", param);
    
    // Do work
    int result = DoWork();
    TRACE_INT("result", result);
    
    TRACE_EXIT();
}
```

## View Traces

```bash
# In the system distro
tail -f /mnt/wslg/stderr.log | grep TRACE

# From Windows PowerShell
Get-Content "\\wsl.localhost\<distro>\mnt\wslg\trace.log" -Tail 50

# With grep filtering
tail -f /mnt/wslg/stderr.log | grep "ProcessMonitor"
```

## Log Levels Explained

| Level | Value | Severity |
|-------|-------|----------|
| TRACE | 1 | Highest verbosity - function entry/exit |
| DEBUG | 2 | Diagnostic information |
| EXCEPTION | 3 | Exception conditions |
| ERROR | 4 | Error conditions |
| INFO | 5 | General information (default) |

**Lower numbers = more verbose**

## Files

- `WSLGd/common.h` - Core logging macros
- `WSLGd/trace.h` - Advanced tracing configuration
- `docs/TRACING.md` - Full tracing guide
- `docs/TRACING_EXAMPLES.md` - Code examples
