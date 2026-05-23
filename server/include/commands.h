#ifndef COMMANDS_H
#define COMMANDS_H

#include "client_registry.h"

int handle_command(ActiveClient *client, const char *cmd);

#endif /* COMMANDS_H */
