#include "spi_event.h"

static void log_spi_event(const struct app_event_header *aeh)
{
    struct spi_event *event = cast_spi_event(aeh);
    APP_EVENT_MANAGER_LOG(aeh, "comm_type=%d", event->type);
}

APP_EVENT_TYPE_DEFINE(spi_event,                                                    /* Unique event name. */
                    NULL,                                                  /* Function logging event data. */
                    NULL,                                                           /* No event info provided. */
                    APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));  /* Flags managing event type. */
