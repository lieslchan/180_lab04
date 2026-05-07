from scipy.interpolate import RectBivariateSpline
import numpy as np
import matplotlib.pyplot as plt

T_vals = [2, 4, 8, 16]
N_vals = [4000, 8000, 16000]
Z = np.array([
    [14.411123, 11.7917,   11.838259, 8.789811],
    [54.444728, 49.472542, 40.707523, 40.23013],
    [179.305995, 204.152498, 153.006995, 163.527195],
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