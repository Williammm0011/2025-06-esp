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
log = logging.getLogger("udp_server")


def get_broadcast_ip():
    try:
        with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as s:
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
        parts = ip.split('.')
        parts[-1] = '255'
        b = '.'.join(parts)
        log.info(f"Broadcast IP: {b}")
        return b
    except Exception as e:
        fallback = "255.255.255.255"
        log.warning(f"Using fallback {fallback} (err: {e})")
        return fallback


def broadcast_start(b_sock, b_ip, port, session_id, seq_delays):
    t0 = time.time()
    for seq, delay in seq_delays.items():
        msg = {"cmd": "start", "seq": seq,
               "delay_ms": delay, "session": session_id}
        data = json.dumps(msg).encode()
        b_sock.sendto(data, (b_ip, port))
        log.info(f"[→START] {msg}")
        time.sleep(0.05)
    return t0  # return timestamp when first send started


def collect_acks(ack_port, expected, timeout=1.0):
    received = set()
    last_ts = None
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("", ack_port))
        s.settimeout(timeout)
        start = time.time()
        while time.time() - start < timeout:
            try:
                data, addr = s.recvfrom(1024)
                msg = json.loads(data.decode())
                ts = time.time()
                log.info(f"[←ACK] {addr}: {msg}")
                dev = msg.get("id")
                if dev in expected:
                    received.add(dev)
                    last_ts = ts
            except socket.timeout:
                break
            except Exception as e:
                log.error(f"ACK error: {e}")
    return received, last_ts


def broadcast_stop(b_ip, port, tries=3):
    stop_msg = {"cmd": "stop", "reason": "missing_acks"}
    data = json.dumps(stop_msg).encode()
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        for i in range(tries):
            s.sendto(data, (b_ip, port))
            log.info(f"[→STOP x{i+1}] {stop_msg}")
            time.sleep(0.05)


def main():
    PORT_START = 12345
    PORT_ACK = 3333
    seq_delays = {0: 10000, 1: 9950, 2: 9900}
    # seq_delays = {0: 10000}
    # expected = {"ESP32_ABC123", "ESP32_DEF456", "ESP32_GHI789"}
    expected = {"ESP32_ABC123"}
    session_id = int(time.time())

    b_ip = get_broadcast_ip()
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as b_sock:
        b_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        t0 = broadcast_start(b_sock, b_ip, PORT_START, session_id, seq_delays)

    received, last_ack_ts = collect_acks(PORT_ACK, expected, timeout=1.0)
    missing = expected - received

    if missing:
        log.warning(f"Missing ACKs: {missing}")
        broadcast_stop(b_ip, PORT_START, tries=3)
    else:
        interval = (last_ack_ts - t0) if last_ack_ts else 0.0
        log.info(f"All ACKs received in {interval*1000:.1f} ms → PROCEED")


if __name__ == "__main__":
    main()
