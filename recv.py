import os
import sys
import json
import pandas as pd
import numpy as np
from threading import Thread
from time import sleep, time
from matplotlib import pyplot as plt
from matplotlib.widgets import Button
from io import StringIO
import serial
import serial.tools.list_ports

### Configuration ###
BAUD_RATE = 115200
CLIP_SIZE = 100
COLLECT_DURATION = 5
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


def read_serial():
    global csi_buffer
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        raise Exception("No serial ports found")
    serial_port = ports[0].device

    ser = serial.Serial(serial_port, BAUD_RATE)
    print(f"Reading from serial port {serial_port} at {BAUD_RATE} baud")

    while True:
        line = ser.readline().decode().strip()
        if "CSI_DATA" in line:
            csidata = pd.read_csv(StringIO(line), header=None)
            csi_buffer = pd.concat([csi_buffer, csidata], ignore_index=True)

            if csi_path is not None:
                csidata.to_csv(csi_path, index=False, header=False, mode='a')

            if len(csi_buffer) >= CLIP_SIZE:
                csi_buffer = csi_buffer.iloc[-CLIP_SIZE:].reset_index(
                    drop=True)

            global last_time, frame_count, fps
            frame_count += 1
            if time() - last_time > 1:
                fps = frame_count / (time() - last_time)
                last_time = time()
                frame_count = 0

            update(csi_buffer)
        else:
            print(line, end=None)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        csi_buffer = pd.read_csv(sys.argv[1], header=None)
        update(csi_buffer)
    else:
        Thread(target=read_serial, daemon=True).start()

    plt.show()
