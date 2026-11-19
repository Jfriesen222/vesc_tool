import sys, struct, time
import canopen
from canopen.sdo.exceptions import SdoAbortedError

net = canopen.Network()
net.connect(interface='socketcan', channel='vcan0')
node = canopen.RemoteNode(5, canopen.ObjectDictionary())  # no EDS: raw index/subindex access
net.add_node(node)

fails = 0
def check(name, ok, info=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + (" " + str(info) if info else ""))
    if not ok: fails += 1

def u32(b): return struct.unpack('<I', b)[0]

st = node.nmt.wait_for_heartbeat(3)
check("heartbeat seen, state", st == 'PRE-OPERATIONAL', st)

v = u32(node.sdo.upload(0x1000, 0))
check("SDO read 0x1000 device type", v == 0x00020192, hex(v))
v = u32(node.sdo.upload(0x1018, 1))
check("SDO read 0x1018:1 vendor id", v == 0xABCD, hex(v))

node.sdo.download(0x2000, 0, struct.pack('<i', -12345))
v = struct.unpack('<i', node.sdo.upload(0x2000, 0))[0]
check("SDO write/read 0x2000 i32", v == -12345, v)

try:
    node.sdo.download(0x1000, 0, struct.pack('<I', 1))
    check("write to read-only aborts", False)
except SdoAbortedError as e:
    check("write to read-only aborts 0x06010002", e.code == 0x06010002, hex(e.code))

try:
    node.sdo.upload(0x3000, 0)
    check("missing object aborts", False)
except SdoAbortedError as e:
    check("missing object aborts 0x06020000", e.code == 0x06020000, hex(e.code))

try:
    node.sdo.download(0x1017, 0, struct.pack('<I', 100))   # 4 bytes into u16
    check("length mismatch aborts", False)
except SdoAbortedError as e:
    check("length mismatch aborts 0x06070010", e.code == 0x06070010, hex(e.code))

# Change heartbeat period to 100 ms and measure it
node.sdo.download(0x1017, 0, struct.pack('<H', 100))
node.nmt.wait_for_heartbeat(2)            # let the old sleep run out
node.nmt.wait_for_heartbeat(2)
t0 = time.time()
for _ in range(5): node.nmt.wait_for_heartbeat(2)
period = (time.time() - t0) / 5
check("heartbeat period ~100 ms after write to 0x1017", 0.07 < period < 0.15, "%.0f ms" % (period * 1000))

node.nmt.state = 'OPERATIONAL'
node.nmt.wait_for_heartbeat(2)
st = node.nmt.wait_for_heartbeat(2)
check("NMT start -> OPERATIONAL", st == 'OPERATIONAL', st)

node.nmt.state = 'STOPPED'
node.nmt.wait_for_heartbeat(2)
st = node.nmt.wait_for_heartbeat(2)
check("NMT stop -> STOPPED", st == 'STOPPED', st)

try:
    node.sdo.RESPONSE_TIMEOUT = 0.3
    node.sdo.upload(0x1000, 0)
    check("no SDO while STOPPED", False)
except canopen.SdoCommunicationError:
    check("no SDO while STOPPED", True)

node.nmt.send_command(0x81)               # reset node
try:
    node.nmt.wait_for_bootup(2)
    check("NMT reset -> boot-up message", True)
except Exception as e:
    check("NMT reset -> boot-up message", False, e)

net.disconnect()
print("ALL PASS" if fails == 0 else "%d FAILED" % fails)
sys.exit(fails)
