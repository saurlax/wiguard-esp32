import os
import sys
import json
import pandas as pd
import numpy as np
from threading import Thread, Lock
from time import sleep, time
from matplotlib import pyplot as plt
from matplotlib.widgets import Button
from matplotlib.animation import FuncAnimation
from io import StringIO
import serial
import serial.tools.list_ports
from sklearn.decomposition import PCA
import paho.mqtt.client as mqtt


### Configuration ###
# BAUD_RATE = 115200
BAUD_RATE = 921600
CLIP_SIZE = 100
COLLECT_DURATION = 5
CMAP_MAX = 128
#####################

# The buffer stores all the CSI data shown on the plot
csidata_buf = np.empty((0, 0))
buffer_lock = Lock()

# Used to store the path of the file to save the CSI data
# None means not ongoing saving, which can avoid multiple saving at the same time
csi_path = None

# Create subplots for heatmap, complex plane, and RSSI display modes
fig, ((ax_heatmap, ax_complex), (ax_rssi, ax_empty)) = plt.subplots(
    2, 2, figsize=(9, 6), gridspec_kw={'height_ratios': [3, 1]}, num='ESP CSI Display')

# Heatmap setup
ax_heatmap.set_title('Heatmap')
ax_heatmap.set_ylabel('Subcarrier Index')
ax_heatmap.set_xlim(0, CLIP_SIZE)
heatmap = ax_heatmap.imshow([[0]], cmap='jet',
                            interpolation='nearest', aspect='auto', origin='lower')
heatmap.set_clim(vmin=0, vmax=CMAP_MAX)
cbar = plt.colorbar(heatmap, ax=ax_heatmap, label='Amplitude')

# Complex plane setup
ax_complex.set_title('Complex Plane')
ax_complex.set_xlim(-CMAP_MAX, CMAP_MAX)
ax_complex.set_ylim(-CMAP_MAX, CMAP_MAX)
ax_complex.axhline(0, color='black', linewidth=0.5)
ax_complex.axvline(0, color='black', linewidth=0.5)
ax_complex.grid(True, which='both', linestyle='--', linewidth=0.5)

# Initialize plot lines and circle for complex plane
sc = ax_complex.scatter([], [], c=[], cmap='viridis')
circle = plt.Circle((0, 0), 0, color='red', linestyle='--', fill=False)
ax_complex.add_artist(circle)

# RSSI setup
ax_rssi.set_xlabel('Packet Index')
ax_rssi.set_ylabel('RSSI')
ax_rssi.set_xlim(0, CLIP_SIZE)
ax_rssi.set_ylim(-60, -20)  # Adjusted height range for RSSI
rssi_line, = ax_rssi.plot([], [], 'r-')

ax_pca = ax_rssi.twinx()
ax_pca.set_ylabel('PCA')
ax_pca.set_ylim(-128, 128)
pca_line, = ax_pca.plot([], [], 'b-')

# Hide the empty subplot
ax_empty.axis('off')

last_time = time()
frame_count = 0
freq = 0
stats = ax_empty.text(0, 0, '', transform=ax_empty.transAxes, color='black')

rssi_data = []

# MQTT setup
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set("admin", "admin")
mqtt_client.tls_set("emqxsl-ca.crt")
mqtt_client.connect("vfbc41b7.ala.cn-hangzhou.emqxsl.cn", 8883)
mqtt_client.publish("user/admin", "Hello, World!")


def update(frame):
    global csidata_buf, rssi_data
    with buffer_lock:
        if len(csidata_buf) == 0:
            return

        # Update heatmap
        amplitude = np.abs(csidata_buf).T
        shape = amplitude.shape
        heatmap.set_data(amplitude)
        heatmap.set_extent((0, shape[1], 0, shape[0]))
        ax_heatmap.set_ylim(0, shape[0])

        # Update complex plane
        real_data = csidata_buf[-1].real
        imag_data = csidata_buf[-1].imag
        colors = np.angle(csidata_buf[-1])
        sc.set_offsets(np.c_[real_data, imag_data])
        sc.set_array(colors)
        circle.set_radius(np.mean(np.abs(csidata_buf)))

        # Update RSSI
        rssi_line.set_data(range(len(rssi_data)), rssi_data)
        pca_line.set_data(range(len(rssi_data)), PCA(
            n_components=1).fit_transform(amplitude.T))

        # 同时使用 mqtt发送数据
        # mqtt_client.publish("user/admin", str(rssi_data[-1]))

        stats.set_text(
            f"""
AMPL: {np.min(np.abs(csidata_buf)):.2f}-{np.max(np.abs(csidata_buf)):.2f}
AVE: {np.mean(np.abs(csidata_buf)):.2f}
FREQ: {freq:.2f}
""")


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


button_ax = plt.axes(
    [ax_empty.get_position().x0, ax_empty.get_position().y1 - 0.05, 0.15, 0.05])
button = Button(button_ax, 'Collect CSI')
button.on_clicked(lambda _: Thread(target=collect_once).start())


def read_serial():
    global csidata_buf, rssi_data
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        raise Exception("No serial ports found")
    serial_port = ports[0].device

    ser = serial.Serial(serial_port, BAUD_RATE, rtscts=True)
    print(f"Reading from serial port {serial_port} at {BAUD_RATE} baud")

    while True:
        try:
            line = ser.readline().decode().strip()
            if line.startswith("CSI_DATA"):
                csi = pd.read_csv(StringIO(line), header=None)
                csidata = json.loads(csi.iloc[0, -1])

                csidata = np.array(csidata)
                csidata = csidata[::2]*1j + csidata[1::2]

                with buffer_lock:
                    if len(csidata_buf) == 0:
                        csidata_buf = np.array([csidata])
                    else:
                        csidata_buf = np.vstack((csidata_buf, [csidata]))

                    rssi_value = csi.iloc[0, 3]
                    rssi_data.append(rssi_value)

                    if csi_path is not None:
                        csi.to_csv(csi_path, index=False,
                                   header=False, mode='a')

                    if len(csidata_buf) >= CLIP_SIZE:
                        csidata_buf = csidata_buf[-CLIP_SIZE:]
                        rssi_data = rssi_data[-CLIP_SIZE:]

                    global last_time, frame_count, freq
                    frame_count += 1
                    if time() - last_time > 1:
                        freq = frame_count / (time() - last_time)
                        last_time = time()
                        frame_count = 0

            else:
                print(line, end=None)
        except Exception as e:
            print(e)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        csidata_buf = pd.read_csv(sys.argv[1], header=None)
        rssi_data = csidata_buf.iloc[:, 3]
        csidata_buf = np.array(csidata_buf.iloc[:, -1].apply(json.loads))
        csidata_buf = np.array([np.array(csi[::2])*1j + csi[1::2]
                                for csi in csidata_buf])

        print(csidata_buf.shape, csidata_buf)
        ampl = np.abs(csidata_buf)
        print(ampl.shape, ampl)
        pca = PCA(n_components=1).fit_transform(ampl)
        print(pca.shape, pca)
        update(None)
    else:
        Thread(target=read_serial, daemon=True).start()

    ani = FuncAnimation(fig, update, interval=33,
                        cache_frame_data=False)  # Update plot at ~30 FPS
    plt.show()
