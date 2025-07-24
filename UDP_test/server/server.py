import socket
import time
import json

# socket.AF_INET = IPv4, socket.SOCK_DGRAM = UDP
broadcast_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Enable broadcast mode
broadcast_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

BROADCAST_IP = "255.255.255.255"
START_PORT = 12345
session_id = 42

# Send start commands to devices
for seq, delay in enumerate([10000, 9950, 9900]):
    msg = {
        "cmd": "start",
        "seq": seq,
        "delay_ms": delay,
        "session": session_id
    }
    data = json.dumps(msg).encode('utf-8')
    broadcast_sock.sendto(data, (BROADCAST_IP, START_PORT)
                          )  # sendto: send UDP packet
    time.sleep(0.05)  # space packets by 50ms


# Wait for ACKs from devices
ACK_PORT = 3333
# the datatype is a set for fast membership testing
EXPECTED_IDS = {"ESP32_A", "ESP32_B", "ESP32_C"}

ack_sock = socket.socket(
    socket.AF_INET, socket.SOCK_DGRAM)  # create UDP socket
ack_sock.bind(("", ACK_PORT))  # bind to all interfaces on port 3333
ack_sock.settimeout(0.5)  # timeout after 0.5 seconds if no data

received_acks = set()
start_time = time.time()

print("[Server] Waiting for ACKs...")
while time.time() - start_time < 0.5:
    try:
        data, addr = ack_sock.recvfrom(1024)
        # recvfrom: receive UDP packet and sender address, 1024 is the buffer size
        message = json.loads(data.decode())
        print(f"[ACK] From {addr}: {message}")
        device_id = message.get("id")
        if device_id:
            received_acks.add(device_id)
    except socket.timeout:
        break  # exit loop if timeout


# Check if all expected ACKs were received
missing = EXPECTED_IDS - received_acks
if missing:
    print(f"[WARN] Missing ACKs from: {missing}")
    stop_msg = {
        "cmd": "stop",
        "reason": "missing_acks",
        "session": session_id
    }
    stop_data = json.dumps(stop_msg).encode()
    stop_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    stop_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    stop_sock.sendto(stop_data, (BROADCAST_IP, START_PORT)
                     )  # broadcast stop command
else:
    print("[OK] All ACKs received.")
