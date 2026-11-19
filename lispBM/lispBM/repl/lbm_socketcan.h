/*
    Copyright 2026    Joel Svensson  svenssonjoel@yahoo.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LBM_SOCKETCAN_H_
#define LBM_SOCKETCAN_H_

#include <stdbool.h>

// Register the SocketCAN extensions. Closes any open CAN socket so that
// a REPL reset starts from a clean state. Call after the VESC stubs are
// loaded so that the real implementations override the stubs.
bool lbm_socketcan_load_extensions(void);

// Close the socket and stop the RX thread.
void lbm_socketcan_close(void);

#endif
