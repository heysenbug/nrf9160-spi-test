#include "ui_terminal_event.h"

static void log_ui_terminal_event(const struct app_event_header *aeh)
{
    struct ui_terminal_event *event = cast_ui_terminal_event(aeh);
    APP_EVENT_MANAGER_LOG(aeh, "SPI done=%d", event->accept_input);
}

APP_EVENT_TYPE_DEFINE(ui_terminal_event,                                                    /* Unique event name. */
                    NULL,                                                  /* Function logging event data. */
                    NULL,                                                           /* No event info provided. */
                    APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));  /* Flags managing event type. */
