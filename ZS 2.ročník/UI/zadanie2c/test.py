import threading
import time

import numpy as np
import random
import matplotlib.pyplot as plt
from scipy.spatial.distance import cdist

clusters = set()
count = 0

np.random.seed = 42
while len(clusters) != 20:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)
    clusters.add((x, y))




count = len(clusters) #toto ešte optimalizovať
while len(clusters) != 1020:
    c = random.choice(list(clusters))

    bod_x, bod_y = c[0],c[1]
    #treba ocekovat ci mozeme mat interval -100 až 100 alebo nie
    x_offset_1, x_offset_2 = -100,100
    y_offset_1, y_offset_2 = -100,100

    if bod_x + x_offset_1 < -5000:
        x_offset_1 = -(bod_x + 5000)

    if bod_x + x_offset_2 > 5000:
        x_offset_2 = 5000 - bod_x

    if bod_y + y_offset_1 < -5000:
        y_offset_1 = -(bod_y + 5000)

    if bod_y + y_offset_2 > 5000:
        y_offset_2 = 5000 - bod_y

    x_offset = random.randint(x_offset_1,x_offset_2)
    y_offset = random.randint(y_offset_1,y_offset_2)
    new_x, new_y = bod_x + x_offset, bod_y + y_offset

    clusters.add((new_x,new_y))


points = list(clusters)


# Funkcia na výpočet centroidu
def calculate_centroid(cluster):
    return np.mean(cluster, axis=0)


# Aglomeratívne klastrovanie s centroidmi
def agglomerative_clustering(points, num_clusters):
    clusters = [[point] for point in points]  # Každý bod je svoj vlastný cluster
    while len(clusters) > num_clusters:
        start = time.time()
        # Vypočítajte centroidy
        centroids = np.array([calculate_centroid(cluster) for cluster in clusters])

        # Vypočítajte vzdialenosti medzi centroidmi
        distances = cdist(centroids, centroids)
        np.fill_diagonal(distances, np.inf)  # Nastavte diagonálne vzdialenosti na nekonečno

        # Nájdite najmenšiu vzdialenosť
        i, j = np.unravel_index(np.argmin(distances), distances.shape)

        # Zlúčte dva clustery
        new_cluster = clusters[i] + clusters[j]
        clusters = [clusters[k] for k in range(len(clusters)) if k != i and k != j]
        clusters.append(new_cluster)

        print("Time elapsed:", time.time() - start)


    return clusters


# Spustenie aglomeratívneho klastrovania
num_clusters = 10
#t = threading.Thread(target=agglomerative_clustering, args=(points, num_clusters))
#t.start()

results = agglomerative_clustering(points, num_clusters)

# Vizualizácia výsledkov
def plot_clusters(clusters):
    for cluster in clusters:
        cluster_array = np.array(cluster)
        plt.scatter(cluster_array[:, 0], cluster_array[:, 1], s=10)
    plt.title("Aglomeratívne klastrovanie")
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.show()


plot_clusters(results)
