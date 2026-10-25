#pragma once

struct GLFWmonitor;

#ifdef __cplusplus
extern "C" {
#endif
int aho_get_cocoa_monitor_size(struct GLFWmonitor* monitor, int* width, int* height);
#ifdef __cplusplus
}
#endif
