import numpy as np
import random
import time
import math
from scipy.spatial.distance import pdist, squareform

import pandas as pd
from tabulate import tabulate

#1. zvol 20 bodov náhodných
clusters = set()
count = 0
while len(clusters) != 20:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)
    clusters.add((x, y))




count = len(clusters) #toto ešte optimalizovať
while len(clusters) != 100:
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


#print(clusters)

clusters = np.array(list(clusters))

#print(clusters)

#tvorba matice

#matica_vzd = np.empty((20020,20020))
#matica_vzd = [[0]*1020]*1020
#differences = clusters[:, np.newaxis, :] - clusters[np.newaxis, :, :]

#distances = np.linalg.norm(clusters[:, np.newaxis] - clusters,axis=2)
matica_vzd = squareform(pdist(clusters))

#print(matica_vzd)
def calculate_distance(bod1,bod2): #funkcia v C, ovela rychlejsia ako normalny python
    return np.linalg.norm(np.array(bod2) - np.array(bod1))


df = pd.DataFrame(matica_vzd[:100, :100])
print(tabulate(df, headers='keys', tablefmt='psql'))


cluster = (  ( (1,2),(2,3),(3,4),(8,9),(4,6),(9,3),(1,4)             ), (4,5)                )
print(cluster[0])


def calculate_centroid2(body):
    centroid = np.mean(body, axis=0)
    #c = (int(centroid[0]), int(centroid[1])) #premena na int, lebo máme pixely a potrebujeme celé súradnice
    return centroid





def find_min(matica):
    """non_zero_indices = np.where(matica != 0) #indexy hodnôt matice, ktoré nie sú 0

    non_zero_values = matica[non_zero_indices] #extrahuje hodnoty s touto maskou
    minimum = np.nanmin(non_zero_values) #minimum z hodnot ktore nie su 0 a ani NaN

    min_index_flat = np.flatnonzero(matica == minimum) #vráti celé indexy všetkých miest, kde je hodnota rovnaká ako naše minimum
    min_index = np.unravel_index(min_index_flat[0], matica.shape) #zistí druhý index v 2d poli"""

    #dolny_troj = np.tril(matica)
    #masked_troj = np.ma.masked_equal(dolny_troj, 0)


    dolny_troj_indexy = np.tril_indices(matica.shape[0], -1)

    dolny_troj_values = matica[dolny_troj_indexy]

    valid_mask = (dolny_troj_values != 0) & (~np.isnan(dolny_troj_values)) #maska pre hodnoty, ktoré nie su 0 ani nan
    valid_values = dolny_troj_values[valid_mask] #extrahovanie hodnot podla masky

    #df = pd.DataFrame(valid_values)
    #print(tabulate(df, headers='keys', tablefmt='psql'))


    minimum = np.nanmin(valid_values)
    #min_index_flat = np.flatnonzero(dolny_troj_values == minimum) #index v riadku

    valid_indices = np.where(valid_mask)[0]
    min_position_in_valid = valid_indices[np.argmin(valid_values)]

    min_index = (dolny_troj_indexy[0][min_position_in_valid], dolny_troj_indexy[1][min_position_in_valid])
    #print(minimum)

    print(minimum, min_index)
    return minimum, min_index[0], min_index[1]




"""def find_min2(matica):
    n = len(matica)
    minimum = float('inf')  # Inicializácia na nekonečno
    row_min, col_min = -1, -1  # Inicializácia indexov

    # Prechádzame iba horný trojuholník matice (kde j > i)
    for i in range(1, n):
        for j in range(i):
            if matica[i][j] < minimum and matica[i][j] != 0.0:
                minimum = matica[i][j]
                row_min, col_min = i, j

    print("Minimum:", minimum, "at position (", row_min, ",", col_min, ")")
    return minimum, row_min, col_min"""


def find_avg_distance(c):
    print("\n# rátam avg #\n")
    centroid_x, centroid_y = calculate_centroid2(c)

    vzdialenosti = 0

    for body in c:
        d = calculate_distance([centroid_x,centroid_y],[body[0],body[1]])
        print("d: ",centroid_x,centroid_y,body[0],body[1])
        vzdialenosti += d
        print("added: ",body, " d: ", d)

    avg = vzdialenosti/len(c)
    print("centroid: ", centroid_x, centroid_y)
    print("vzd: ",avg, " poc: ", len(c))



    if avg <= 500:
        print("cluster je ok")
        print("\n# koniec avg #\n")
        return 0
    else:
        print("cluster je nie ok ", avg)
        print("\n# koniec avg #\n")
        return 1


def find_avg_distance2(cluster):
    centroid = calculate_centroid2(cluster)

    print("centroid: ", centroid)

    points = np.array(cluster)
    distances = np.linalg.norm(points - centroid, axis=1)

    # Vypočítame priemernú vzdialenosť
    avg_distance = np.mean(distances)

    print("avg d: ", avg_distance)
    if avg_distance <= 500:
        print("cluster je ok")
        return 0
    else:
        print("cluster je nie ok ", avg_distance)
        return 1










print("zaciname")
start = time.time()

#minimum, i1, i2 = find_min(matica_vzd)

#find_min(matica_vzd)
#minimum,i1,i2 = find_min(matica_vzd)

find_avg_distance(cluster[0])
find_avg_distance2(cluster[0])


end = time.time()
print("Time elapsed: ", end-start, "s")












#generacia bodov cca 2.15s
#matica vyplnenie cez numpy funkciu: 82s
#matica vyplnenie cez scipy 2.2s ???????? to je crazy

#najdenie minima s maskou dolneho trojuholnika 16s-25s na 20000 bodov
#najdenie minima s indexami dolneho trojuholnika 8-15s cca na 20000 bodov co je crazy (bez youtube aj 3-5s)