import serial
import time
import serial.tools.list_ports

COLLECT_INTERVAL = 5
timestamp = time.time()


ports = list(serial.tools.list_ports.comports())
print(f"Using port: {ports[0].device}")
io = serial.Serial(ports[0].device, 115200)
while True:
    try:
        line = io.readline().decode()
        print(line, end="")
        if line.startswith("CSI_DATA"):
            now = time.time()
            if now - timestamp > COLLECT_INTERVAL:
                timestamp = now
            with open(f"./csi/csi-{int(timestamp)}.csv", "ab") as file:
                file.write(line.encode())
    except KeyboardInterrupt:
        break
    except Exception as e:
        print(f"Error: {e}")
        continue

io.close()
