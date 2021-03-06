#ifndef KERNEL_LIB_H
#define KERNEL_LIB_H

#include <stdint.h>

char *cpuVendor(char *result);

int64_t read(uint64_t fd, char *buffer, uint64_t maxBytes);
int64_t write(uint64_t fd, char *buffer, uint64_t maxBytes);

int64_t sys_read(uint8_t fd, char *buff, uint32_t maxBytes);
int64_t sys_write(uint8_t fd, char *buff, uint32_t maxBytes);
void sys_sound(uint32_t freq, uint32_t time);

int64_t read(uint64_t fd, char *buffer, uint64_t maxBytes);
int64_t write(uint64_t fd, char *buffer, uint64_t maxBytes);

/**
* Prints the specified number of spaces. Used for aligning printMQs() and printIPCs() output.
*/
void pad(uint64_t spaces);

#endif