#ifndef _DRIVERS_ACPI_H
#define _DRIVERS_ACPI_H

#include <stdint.h>
#include <stdbool.h>

void acpi_init(void);
void acpi_poweroff(void);
void acpi_reboot(void);

#endif // _DRIVERS_ACPI_H
