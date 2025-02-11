import os
import sys
import socket
import json
import pandas as pd
import numpy as np
from threading import Thread, Event
from time import sleep, time
from matplotlib import pyplot as plt
from matplotlib.widgets import Button
from io import StringIO

### Configuration ###
PORT = 8000
CLIP_SIZE = 100
COLLECT_DURATION = 5
PING_FREQUENCY = 1
#####################

# The buffer stores all the CSI data shown on the plot
csi_buffer = pd.DataFrame()

# Used to store the path of the file to save the CSI data
# None means not ongoing saving, which can avoid multiple saving at the same time
csi_path = None

fig, ax = plt.subplots(num='ESP CSI Amplitude Heatmap')
plt.xlabel('Packet Index')
plt.ylabel('Subcarrier Index')

heatmap = ax.imshow([[0]], cmap='jet',
                    interpolation='nearest', aspect='auto', origin='lower')
heatmap.set_clim(vmin=0, vmax=64)
cbar = plt.colorbar(heatmap, ax=ax, label='Amplitude')

last_time = time()
frame_count = 0
fps = 0
stats = ax.text(0.02, 0.95, '', transform=ax.transAxes, color='white')


def update(csi: pd.DataFrame):
    csidata = np.stack(csi.iloc[:, -1].apply(json.loads).apply(np.array)).T
    csidata = csidata[::2]+csidata[1::2]*1j

    amplitude = np.abs(csidata)
    shape = amplitude.shape
    heatmap.set_data(amplitude)
    heatmap.set_extent((0, shape[1], 0, shape[0]))

    ax.set_xlim(0, shape[1])
    ax.set_ylim(0, shape[0])

    global last_time, frame_count, fps
    frame_count += 1
    if time() - last_time > 1:
        fps = frame_count / (time() - last_time)
        last_time = time()
        frame_count = 0

    stats.set_text(
        f"MIN: {np.min(amplitude):.2f} MAX: {np.max(amplitude):.2f} MEAN: {np.mean(amplitude):.2f} FPS: {fps:.2f}")

    plt.draw()


def collect_once():
    '''
    Collect CSI data for a certain duration and save it to a file.
    '''
    global csi_path
    if csi_path is not None:
        return
    csi_path = os.path.join('data', f'csi-{int(time())}.csv')
    print(f"Collecting CSI to {csi_path} for {COLLECT_DURATION}s")
    sleep(COLLECT_DURATION)
    print(f"Collecting CSI to {csi_path} finished")
    csi_path = None


button = Button(plt.axes((0, 0.95, 0.15, 0.05)), 'Collect CSI')
button.on_clicked(lambda _: Thread(target=collect_once).start())


def tcping(client, stop_event, frequency):
    interval = 1.0 / frequency
    while not stop_event.is_set():
        client.send(b'.')
        sleep(interval)


def on_connection(client, addr):
    global csi_buffer
    print(f"Handling connection from {addr}")

    stop_event = Event()
    tcping_thread = Thread(target=tcping, args=(
        client, stop_event, PING_FREQUENCY))
    tcping_thread.start()

    while True:
        data = b''
        # recvline
        while True:
            c = client.recv(1)
            if c == b'\n':
                break
            data += c

        if not data:
            break

        csidata = pd.read_csv(StringIO(data.decode()), header=None)
        csi_buffer = pd.concat([csi_buffer, csidata], ignore_index=True)

        if csi_path is not None:
            csidata.to_csv(csi_path, index=False, header=False, mode='a')

        if len(csi_buffer) >= CLIP_SIZE:
            csi_buffer = csi_buffer.iloc[-CLIP_SIZE:].reset_index(drop=True)

        update(csi_buffer)

    stop_event.set()
    tcping_thread.join()
    client.close()
    print(f"Connection from {addr} closed")


def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('0.0.0.0', PORT))
    server.listen(5)
    print(f"Server listening on port {PORT}")

    while True:
        client, addr = server.accept()
        Thread(target=on_connection, args=(client, addr), daemon=True).start()


if __name__ == '__main__':
    if len(sys.argv) > 1:
        csi_buffer = pd.read_csv(sys.argv[1], header=None)
        update(csi_buffer)
    else:
        Thread(target=start_server, daemon=True).start()

    plt.show()
