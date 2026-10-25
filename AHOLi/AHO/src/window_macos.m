#include "window_macos.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

int aho_get_cocoa_monitor_size(GLFWmonitor* monitor, int* width, int* height) {
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(glfwGetCocoaMonitor(monitor));
    if (!mode)
        return 0;
    *width = (int)CGDisplayModeGetWidth(mode);
    *height = (int)CGDisplayModeGetHeight(mode);
    CGDisplayModeRelease(mode);
    return 1;
}
