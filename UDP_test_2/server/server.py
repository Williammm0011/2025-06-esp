#!/usr/bin/env python3
import socket
import time
import json
import logging
from contextlib import closing

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s"
)
logger = logging.getLogger("udp_server")


def get_broadcast_ip():
    """Determine local network broadcast address, fallback if needed."""
    try:
        with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as s:
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
        parts = local_ip.split('.')
        parts[-1] = '255'
        b_ip = '.'.join(parts)
        logger.info(f"Using broadcast IP: {b_ip}")
        return b_ip
    except Exception as e:
        fallback = "255.255.255.255"
        logger.warning(f"Fallback broadcast IP {fallback} (error: {e})")
        return fallback


def broadcast_start(sock, b_ip, port, session_id, seq_delays):
    """Send sequenced start broadcasts."""
    for seq, delay in seq_delays.items():
        msg = {
            "cmd":      "start",
            "seq":      seq,
            "delay_ms": delay,
            "session":  session_id
        }
        data = json.dumps(msg).encode("utf-8")
        try:
            sock.sendto(data, (b_ip, port))
            logger.info(f"[Broadcast] {msg}")
        except Exception as e:
            logger.error(f"Failed to broadcast {msg}: {e}")
        time.sleep(0.05)  # 50 ms gap


def collect_acks(ack_port, expected_ids, timeout=1.0):
    """Listen for ACKs for up to `timeout` seconds."""
    received = set()
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as ack_sock:
        ack_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        ack_sock.bind(("", ack_port))
        ack_sock.settimeout(timeout)

        start = time.time()
        while time.time() - start < timeout:
            try:
                data, addr = ack_sock.recvfrom(1024)
                msg = json.loads(data.decode("utf-8"))
                logger.info(f"[ACK] From {addr}: {msg}")
                dev_id = msg.get("id")
                if dev_id in expected_ids:
                    received.add(dev_id)
            except socket.timeout:
                break
            except Exception as e:
                logger.error(f"Error receiving ACK: {e}")
    return received


def main():
    BROADCAST_PORT = 12345
    ACK_PORT = 3333
    seq_delays = {0: 10000, 1: 9950, 2: 9900}
    session_id = int(time.time())
    expected_ids = {"ESP32_ABC123", "ESP32_DEF456", "ESP32_GHI789"}

    b_ip = get_broadcast_ip()
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as b_sock:
        b_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        broadcast_start(b_sock, b_ip, BROADCAST_PORT, session_id, seq_delays)

    received = collect_acks(ACK_PORT, expected_ids, timeout=1.0)
    missing = expected_ids - received

    if missing:
        logger.warning(f"Missing ACKs from: {missing} — broadcasting STOP")
        stop_msg = {"cmd": "stop",
                    "reason": "missing_acks", "session": session_id}
        data = json.dumps(stop_msg).encode("utf-8")
        with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as stop_sock:
            stop_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            try:
                stop_sock.sendto(data, (b_ip, BROADCAST_PORT))
                logger.info("[Broadcast] STOP command sent")
            except Exception as e:
                logger.error(f"Failed to send STOP: {e}")
    else:
        logger.info("✅ All ACKs received — devices will start.")


if __name__ == "__main__":
    main()
