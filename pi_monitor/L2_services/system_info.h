/**
 * @file system_info.h
 * @brief System Information Interface (Layer 3)
 */

#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int percent;
} cpu_info_t;

typedef struct {
    int percent;
    int used_mb;
    int total_mb;
} mem_info_t;

typedef struct {
    float celsius;
    int percent;
} temp_info_t;

typedef struct {
    int percent;
    int used_gb;
    int total_gb;
} disk_info_t;

typedef struct {
    int rpm;
    int pwm;
    int percent;
} fan_info_t;

void sysinfo_init(void);
void sysinfo_get_cpu(cpu_info_t* info);
void sysinfo_get_memory(mem_info_t* info);
void sysinfo_get_temperature(temp_info_t* info);
void sysinfo_get_disk(disk_info_t* info);
void sysinfo_get_fan(fan_info_t* info);
void sysinfo_get_ip(char* buf, size_t buflen);
void sysinfo_get_uptime(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif
