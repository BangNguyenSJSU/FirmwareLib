#include "system_info.h"      // System info API declarations
#include <stdio.h>             // File I/O operations (fopen, fscanf, fgets)
#include <stdlib.h>            // Standard library (unused, kept for future use)
#include <string.h>            // String operations (strcmp, strncmp, strcpy)
#include <sys/statvfs.h>       // Filesystem statistics (statvfs)
#include <sys/sysinfo.h>       // System information (sysinfo struct)
#include <ifaddrs.h>           // Network interface addresses (getifaddrs)
#include <netinet/in.h>        // Internet address family (sockaddr_in)
#include <arpa/inet.h>         // IP address conversion (inet_ntop)

#define FAN_RPM_PATH "/sys/class/hwmon/hwmon2/fan1_input"
#define FAN_PWM_PATH "/sys/class/hwmon/hwmon2/pwm1"

static unsigned long prev_idle = 0;
static unsigned long prev_total = 0;

void sysinfo_init(void) {
    prev_idle = 0;
    prev_total = 0;
    cpu_info_t cpu;
    sysinfo_get_cpu(&cpu);
}

void sysinfo_get_cpu(cpu_info_t* info) {
    if (!info) return;
    info->percent = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    char line[256];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return; }
    fclose(fp);
    unsigned long user, nice, system, idle, iowait, irq, softirq;
    sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    unsigned long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long idle_time = idle + iowait;
    unsigned long total_diff = total - prev_total;
    unsigned long idle_diff = idle_time - prev_idle;
    prev_total = total;
    prev_idle = idle_time;
    if (total_diff == 0) return;
    info->percent = (int)(100 * (total_diff - idle_diff) / total_diff);
}

void sysinfo_get_memory(mem_info_t* info) {
    if (!info) return;
    info->percent = 0; info->used_mb = 0; info->total_mb = 0;
    struct sysinfo si;
    if (sysinfo(&si) != 0) return;
    unsigned long total = si.totalram / (1024 * 1024);
    unsigned long free_mem = si.freeram / (1024 * 1024);
    unsigned long buffers = si.bufferram / (1024 * 1024);
    unsigned long cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Cached:", 7) == 0) {
                sscanf(line, "Cached: %lu", &cached);
                cached /= 1024;
                break;
            }
        }
        fclose(fp);
    }
    unsigned long used = total - free_mem - buffers - cached;
    info->total_mb = (int)total;
    info->used_mb = (int)used;
    if (total > 0) info->percent = (int)(100 * used / total);
}

void sysinfo_get_temperature(temp_info_t* info) {
    if (!info) return;
    info->celsius = 0.0f; info->percent = 0;
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return;
    int temp;
    if (fscanf(fp, "%d", &temp) == 1) {
        info->celsius = temp / 1000.0f;
        info->percent = (int)((info->celsius / 85.0f) * 100);
        if (info->percent > 100) info->percent = 100;
    }
    fclose(fp);
}

void sysinfo_get_disk(disk_info_t* info) {
    if (!info) return;
    info->percent = 0; info->used_gb = 0; info->total_gb = 0;
    struct statvfs st;
    if (statvfs("/", &st) != 0) return;
    unsigned long total = (st.f_blocks * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long free_space = (st.f_bfree * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long used = total - free_space;
    info->total_gb = (int)total;
    info->used_gb = (int)used;
    if (total > 0) info->percent = (int)(100 * used / total);
}

void sysinfo_get_fan(fan_info_t* info) {
    if (!info) return;
    info->rpm = 0; info->pwm = 0; info->percent = 0;
    FILE *fp = fopen(FAN_RPM_PATH, "r");
    if (fp) { fscanf(fp, "%d", &info->rpm); fclose(fp); }
    fp = fopen(FAN_PWM_PATH, "r");
    if (fp) { fscanf(fp, "%d", &info->pwm); fclose(fp); info->percent = (info->pwm * 100) / 255; }
}

void sysinfo_get_ip(char* buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    strcpy(buf, "No IP");
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, buf, buflen);
        break;
    }
    freeifaddrs(ifaddr);
}

void sysinfo_get_uptime(char* buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    strcpy(buf, "??:??");
    struct sysinfo si;
    if (sysinfo(&si) != 0) return;
    long uptime = si.uptime;
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int mins = (uptime % 3600) / 60;
    if (days > 0) snprintf(buf, buflen, "%dd %dh", days, hours);
    else snprintf(buf, buflen, "%dh %dm", hours, mins);
}
