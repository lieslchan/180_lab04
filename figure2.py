from scipy.interpolate import RectBivariateSpline
import numpy as np
import matplotlib.pyplot as plt

T_vals = [2, 4, 8, 16]
N_vals = [4000, 8000, 16000]

Z = np.array([
    [0.04178,      0.014063,      0.01846833333, 0.015018],
    [0.1190026667, 0.073394,      0.04183966667, 0.03694],
    [0.4202863333, 0.2281216667,  0.1129856667,  0.07026333333]
])

spline = RectBivariateSpline(N_vals, T_vals, Z, kx=2, ky=3)

T_fine = np.linspace(2, 16, 100)
N_fine = np.linspace(4000, 16000, 100)
T_grid, N_grid = np.meshgrid(T_fine, N_fine)
Z_smooth = spline(N_fine, T_fine)

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
ax.plot_surface(T_grid, N_grid, Z_smooth, cmap='coolwarm', edgecolor='none')

ax.set_xlabel('T (threads)')
ax.set_ylabel('N (matrix size)')
ax.set_zlabel('Avg Runtime (s)')

plt.tight_layout()
plt.show()