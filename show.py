#!/usr/bin/env python3
import numpy as np
import matplotlib.pyplot as plt
import sys
import pandas as pd


def to_complex(arr):
    return np.vectorize(complex)(arr[::2], arr[1::2])


data = pd.read_csv(sys.argv[1], header=None)
csidata = data.iloc[:, -1].apply(lambda csi: np.array(eval(csi))).values


csidata = np.vstack([to_complex(csi) for csi in csidata]).T
print(csidata.shape)

amplitude = np.abs(csidata)
plt.imshow(amplitude, cmap='jet', interpolation='nearest', aspect='auto')
plt.colorbar(label='Amplitude')
plt.title('ESP CSI Amplitude Heatmap')
plt.xlabel('Packet Index')
plt.ylabel('Subcarrier Index')
plt.show()
