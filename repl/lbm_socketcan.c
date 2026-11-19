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

/*
  SocketCAN extensions for the Linux REPL.

  The Lisp-facing API mirrors the VESC CAN API so that code written for
  VESC can run unchanged against a real or virtual (vcan) CAN bus:

    (can-open "vcan0")         -> t | nil      REPL only, select interface
    (can-close)                -> t
    (can-send-sid id data)     -> t | nil      11-bit id
    (can-send-eid id data)     -> t | nil      29-bit id
    (event-enable 'event-can-sid [on])         enable/disable rx events
    (event-enable 'event-can-eid [on])

  data is a byte array or a list of numbers, at most 8 bytes.

  Received frames are delivered as events to the registered event handler:
    (event-can-sid . (id . data))
    (event-can-eid . (id . data))
*/

// struct ifreq / IFNAMSIZ are hidden under strict -std=c11.
#define _DEFAULT_SOURCE

#include "lbm_socketcan.h"

#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "lispbm.h"
#include "extensions.h"
#include "lbm_flat_value.h"
#include "platform_thread.h"

#define CAN_RX_POLL_TIMEOUT_MS 100
#define CAN_EVENT_BUF_SIZE     64

static int can_fd = -1;
static lbm_thread_t rx_thread;
static volatile bool rx_running = false;
static volatile bool event_sid_en = false;
static volatile bool event_eid_en = false;
static volatile lbm_uint rx_dropped = 0;

static lbm_uint sym_event_can_sid = 0;
static lbm_uint sym_event_can_eid = 0;

// ---------------------------------------------------------------------------
// RX thread

static void post_frame(struct can_frame *f) {
  bool eid = (f->can_id & CAN_EFF_FLAG) != 0;
  if (eid ? !event_eid_en : !event_sid_en) return;
  if (!lbm_event_handler_exists()) return;

  uint8_t len = f->can_dlc > 8 ? 8 : f->can_dlc;
  uint32_t id = f->can_id & (eid ? CAN_EFF_MASK : CAN_SFF_MASK);

  lbm_flat_value_t fv;
  if (!lbm_start_flatten(&fv, CAN_EVENT_BUF_SIZE)) {
    rx_dropped++;
    return;
  }
  bool ok =
    f_cons(&fv) &&
    f_sym(&fv, eid ? sym_event_can_eid : sym_event_can_sid) &&
    f_cons(&fv) &&
    f_u32(&fv, id) &&
    f_lbm_array(&fv, len, f->data) &&
    lbm_finish_flatten(&fv);
  if (!ok || !lbm_event(&fv)) {
    // Not handed over to the evaluator, so we still own the buffer.
    lbm_free(fv.buf);
    rx_dropped++;
  }
}

static void rx_loop(void *arg) {
  (void)arg;
  struct pollfd pfd = { .fd = can_fd, .events = POLLIN };
  while (rx_running) {
    int r = poll(&pfd, 1, CAN_RX_POLL_TIMEOUT_MS);
    if (r <= 0 || !(pfd.revents & POLLIN)) continue;
    struct can_frame f;
    ssize_t n = read(can_fd, &f, sizeof(f));
    if (n != (ssize_t)sizeof(f)) continue;
    if (f.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG)) continue;
    post_frame(&f);
  }
}

void lbm_socketcan_close(void) {
  if (rx_running) {
    rx_running = false;
    lbm_thread_destroy(&rx_thread); // joins
  }
  if (can_fd >= 0) {
    close(can_fd);
    can_fd = -1;
  }
}

// ---------------------------------------------------------------------------
// Extensions

static lbm_value ext_can_open(lbm_value *args, lbm_uint argn) {
  if (argn != 1 || !lbm_is_array_r(args[0])) return ENC_SYM_TERROR;
  char *ifname = lbm_dec_str(args[0]);
  if (!ifname || strlen(ifname) >= IFNAMSIZ) return ENC_SYM_TERROR;

  lbm_socketcan_close();

  int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (fd < 0) return ENC_SYM_NIL;

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    close(fd);
    return ENC_SYM_NIL;
  }

  struct sockaddr_can addr;
  memset(&addr, 0, sizeof(addr));
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return ENC_SYM_NIL;
  }

  can_fd = fd;
  rx_running = true;
  if (!lbm_thread_create(&rx_thread, "can_rx", rx_loop, NULL,
                         LBM_THREAD_PRIO_NORMAL, 0)) {
    rx_running = false;
    close(can_fd);
    can_fd = -1;
    return ENC_SYM_NIL;
  }
  return ENC_SYM_TRUE;
}

static lbm_value ext_can_close(lbm_value *args, lbm_uint argn) {
  (void)args; (void)argn;
  lbm_socketcan_close();
  return ENC_SYM_TRUE;
}

// Decode data argument (byte array or list of numbers) into buf.
// Returns number of bytes, or -1 on type error.
static int decode_data(lbm_value v, uint8_t *buf) {
  if (lbm_is_array_r(v)) {
    lbm_array_header_t *arr = lbm_dec_array_r(v);
    if (!arr || arr->size > 8) return -1;
    memcpy(buf, arr->data, arr->size);
    return (int)arr->size;
  }
  int n = 0;
  while (lbm_is_cons(v)) {
    lbm_value e = lbm_car(v);
    if (n >= 8 || !lbm_is_number(e)) return -1;
    buf[n++] = (uint8_t)lbm_dec_as_u32(e);
    v = lbm_cdr(v);
  }
  if (!lbm_is_symbol_nil(v)) return -1;
  return n;
}

static lbm_value can_send(lbm_value *args, lbm_uint argn, bool eid) {
  if (argn != 2 || !lbm_is_number(args[0])) return ENC_SYM_TERROR;
  struct can_frame f;
  memset(&f, 0, sizeof(f));
  int len = decode_data(args[1], f.data);
  if (len < 0) return ENC_SYM_TERROR;
  if (can_fd < 0) return ENC_SYM_NIL;

  uint32_t id = lbm_dec_as_u32(args[0]);
  f.can_id = eid ? ((id & CAN_EFF_MASK) | CAN_EFF_FLAG) : (id & CAN_SFF_MASK);
  f.can_dlc = (uint8_t)len;
  ssize_t n = write(can_fd, &f, sizeof(f));
  return n == (ssize_t)sizeof(f) ? ENC_SYM_TRUE : ENC_SYM_NIL;
}

static lbm_value ext_can_send_sid(lbm_value *args, lbm_uint argn) {
  return can_send(args, argn, false);
}

static lbm_value ext_can_send_eid(lbm_value *args, lbm_uint argn) {
  return can_send(args, argn, true);
}

// (event-enable 'event-can-sid [on]). Unknown events return nil, matching
// the behaviour of the VESC stub this overrides.
static lbm_value ext_event_enable(lbm_value *args, lbm_uint argn) {
  if (argn < 1 || argn > 2 || !lbm_is_symbol(args[0])) return ENC_SYM_TERROR;
  bool on = argn == 2 ? !lbm_is_symbol_nil(args[1]) : true;
  lbm_uint s = lbm_dec_sym(args[0]);
  if (s == sym_event_can_sid) {
    event_sid_en = on;
  } else if (s == sym_event_can_eid) {
    event_eid_en = on;
  } else {
    return ENC_SYM_NIL;
  }
  return ENC_SYM_TRUE;
}

// (can-rx-dropped) -> number of received frames that could not be posted
static lbm_value ext_can_rx_dropped(lbm_value *args, lbm_uint argn) {
  (void)args; (void)argn;
  return lbm_enc_u(rx_dropped);
}

static bool get_or_add_symbol(const char *name, lbm_uint *id) {
  // The symbol may already exist, e.g. when restored from an image.
  if (lbm_get_symbol_by_name(name, id)) return true;
  return lbm_add_symbol_const(name, id) != 0;
}

bool lbm_socketcan_load_extensions(void) {
  // A REPL reset reinitializes the evaluator; stop delivering events into
  // the old one and start from a clean state.
  lbm_socketcan_close();
  event_sid_en = false;
  event_eid_en = false;
  rx_dropped = 0;

  bool ok =
    get_or_add_symbol("event-can-sid", &sym_event_can_sid) &&
    get_or_add_symbol("event-can-eid", &sym_event_can_eid);

  ok = ok &&
    lbm_add_extension("can-open", ext_can_open) &&
    lbm_add_extension("can-close", ext_can_close) &&
    lbm_add_extension("can-send-sid", ext_can_send_sid) &&
    lbm_add_extension("can-send-eid", ext_can_send_eid) &&
    lbm_add_extension("can-rx-dropped", ext_can_rx_dropped) &&
    lbm_add_extension("event-enable", ext_event_enable);
  return ok;
}
