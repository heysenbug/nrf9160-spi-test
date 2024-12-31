/*
 * Copyright (c) 2024 Evercars
 */

#ifndef _UI_TERMINAL_EVENT_H_
#define _UI_TERMINAL_EVENT_H_

#include <app_event_manager.h>

struct ui_terminal_event {
    struct app_event_header header;
    bool accept_input;
};

APP_EVENT_TYPE_DECLARE(ui_terminal_event);

#endif /* _UI_TERMINAL_EVENT_H_ */