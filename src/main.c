/*
 * Copyright (c) 2024 Evercars
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

/* Include the header files for SPI, GPIO and devicetree */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

/* CAF Application Event Manager Framework */
#include <app_event_manager.h>

#define MODULE main
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

void print_banner(void)
{
    LOG_INF("**********************");
    LOG_INF("        RICHIE");
    LOG_INF("**********************");
}

int main(void)
{
    print_banner();

    if (app_event_manager_init()) {
        /* Without the Application Event Manager, the application will not work
		 * as intended. A reboot is required in an attempt to recover.
		 */
		LOG_ERR("Application Event Manager could not be initialized, rebooting...");
		k_sleep(K_SECONDS(5));
		sys_reboot(SYS_REBOOT_COLD);
    }

    LOG_INF("AEM Initialization complete!");
    
    return 0;
}
