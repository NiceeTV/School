import numpy as np
import random
import time
import math
from scipy.spatial.distance import pdist, squareform
import matplotlib.pyplot as plt
import pandas as pd
from tabulate import tabulate
import threading

#1. zvol 20 bodov náhodných
clusters = set()
count = 0
np.random.seed = 42 #toto nič nerobi

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

clusters = np.array(list(clusters))


#distances = np.linalg.norm(clusters[:, np.newaxis] - clusters,axis=2)
matica_vzd = squareform(pdist(clusters))



clusters = clusters.tolist() #zmena na list normálne
clusters = [[c] for c in clusters] #obalim body do vlastnych clusterov

#print(matica_vzd)
def calculate_distance(bod1,bod2): #funkcia v C, ovela rychlejsia ako normalny python
    return np.linalg.norm(np.array([bod2]) - np.array([bod1]))


def calculate_centroid2(body):
    if len(body) == 1:
        return body[0]
    else:
        centroid = np.mean(body, axis=0)
        return centroid


def calculate_medoid(body):
    if len(body) == 1:
        return body[0]
    else:

        distances = squareform(pdist(body))

        sum_distances = np.sum(distances, axis=1)

        medoid_index = np.argmin(sum_distances)

        #print(medoid_index)
        medoid = body[medoid_index]

        #print(medoid)

        #print(sum_distances)

        #df = pd.DataFrame(distances)
        #print(tabulate(df, headers='keys', tablefmt='psql'))

        return medoid





def find_min_idx(x):
    i, j = np.unravel_index(np.argmin(matica_vzd), matica_vzd.shape)
    minimum = x[i,j]
    return minimum, i, j



def find_avg_distance2(cluster):
    centroid = calculate_centroid2(cluster)
    points = np.array(cluster)
    distances = np.linalg.norm(points - centroid, axis=1)

    # Vypočítame priemernú vzdialenosť
    avg_distance = np.mean(distances)

    #print("avg d: ", avg_distance)
    if avg_distance <= 500:
        #print("cluster je ok")
        return 0
    else:
        print("cluster je nie ok ", avg_distance)
        return 1


def find_avg_distance_medoid(cluster):
    medoid = calculate_medoid(cluster)
    points = np.array(cluster)
    distances = np.linalg.norm(points - medoid, axis=1)

    # Vypočítame priemernú vzdialenosť
    avg_distance = np.mean(distances)

    #print("avg d: ", avg_distance)
    if avg_distance <= 500:
        #print("cluster je ok")
        return 0
    else:
        print("cluster je nie ok ", avg_distance)
        return 1






def remove_indexes_vzd(pole,i1,i2): #odstranenie riadkov aj stlpcov pre matica_vzd
    riadky, stlpce = pole.shape

    # Vytvorenie masky, ktorá všetko zachová
    mask = np.ones((riadky, stlpce), dtype=bool)

    # Nastavenie hodnôt na False pre riadky a stĺpce, ktoré chceš odstrániť
    mask[[i1,i2], :] = False #maskovanie riadkov
    mask[:, [i1,i2]] = False #maskovanie stlpcov

    #filtered_pole = pole[mask].reshape((riadky - 2, stlpce - 2))  # Zmena tvaru matice
    pole[mask] = np.inf
    return pole



def aglomeratne_centroidne(clusters):
    global matica_vzd

    start = time.time()

    while True:
        #pocet = len([x for x in clusters if x is not None])
        #print("\npocet: ",pocet)

        start2 = time.time()
        najmensia_vzd,i1,i2 = find_min_idx(matica_vzd)

        #end = time.time()
        #print("Najdenie minima: ", end - start, "s")

        if najmensia_vzd == -1:
            print("prazdne pole")
            break

        #zlucenie clusterov
        c1 = clusters[i1]
        c2 = clusters[i2]
        new_cluster = c1 + c2

        #print("old1: ",c1, " old2: ",c2, " new: ",new_cluster)

        #start = time.time()
        #check noveho clustera
        podmienka = find_avg_distance2(new_cluster)
        if podmienka == 1 or len(clusters) == 1:
            print("broke here")
            break

        #end = time.time()
        #print("AVG dist: ", end - start, "s")



        #start = time.time()

        clusters[i2] = None
        clusters[i1] = new_cluster

        #end = time.time()
        #print("Remove clusters idx + odstranenie z clusters: ", end - start, "s")




        #start = time.time()

        matica_vzd[i2, :] = np.inf   # maskovanie riadkov
        matica_vzd[:, [i1, i2]] = np.inf  # maskovanie stlpcov

        #end = time.time()
        #print("Remove z matice_vzd: ", end - start, "s")

        centroid_x, centroid_y = calculate_centroid2(new_cluster)


        #start = time.time()
        distances = np.zeros(matica_vzd.shape[0])


        for index,c in enumerate(clusters):
            if c is not None:
                centroid2_x, centroid2_y = calculate_centroid2(c)
                d = calculate_distance([centroid_x, centroid_y], [centroid2_x, centroid2_y])
                distances[index] = d


        #end = time.time()
        #print("Pocitanie riadka: ", end - start, "s")






        #start = time.time()

        #upraviť riadok v maske

        distances[distances == 0] = np.inf
        matica_vzd[i1] = distances

        #end = time.time()
        #print("Pridanie riadka: ", end - start, "s")

        #print("\n")
        end2 = time.time()
        print("Time elapsed: ", end2 - start2, "s")



    end = time.time()
    print("Time elapsed: ", end - start, "s")

    plot_clusters(clusters)
    return clusters





def aglomeratne_clustering(clusters,mode='centroid'):
    global matica_vzd

    start = time.time()

    if mode != 'centroid' and mode != 'medoid':
        print("Nesprávny mode zhlukovania")
        return clusters


    while True:
        # pocet = len([x for x in clusters if x is not None])
        # print("\npocet: ",pocet)

        start2 = time.time()
        najmensia_vzd, i1, i2 = find_min_idx(matica_vzd)

        # end = time.time()
        # print("Najdenie minima: ", end - start, "s")

        if najmensia_vzd == -1:
            print("prazdne pole")
            break

        # zlucenie clusterov
        c1 = clusters[i1]
        c2 = clusters[i2]
        new_cluster = c1 + c2

        # print("old1: ",c1, " old2: ",c2, " new: ",new_cluster)

        # start = time.time()
        # check noveho clustera

        if mode == 'medoid':
            podmienka = find_avg_distance_medoid(new_cluster)
        else:
            podmienka = find_avg_distance2(new_cluster)


        if podmienka == 1 or len(clusters) == 1:
            print("broke here")
            break

        # end = time.time()
        # print("AVG dist: ", end - start, "s")

        # start = time.time()

        clusters[i2] = None
        clusters[i1] = new_cluster

        # end = time.time()
        # print("Remove clusters idx + odstranenie z clusters: ", end - start, "s")

        # start = time.time()

        matica_vzd[i2, :] = np.inf  # maskovanie riadkov
        matica_vzd[:, [i1, i2]] = np.inf  # maskovanie stlpcov

        # end = time.time()
        # print("Remove z matice_vzd: ", end - start, "s")

        #centroid_x, centroid_y = calculate_centroid2(new_cluster)



        if mode == 'medoid':
            medoid_x, medoid_y = calculate_medoid(new_cluster)
        else:
            centroid_x, centroid_y = calculate_centroid2(new_cluster)


        # start = time.time()
        distances = np.zeros(matica_vzd.shape[0])


        if mode == 'medoid':
            for index, c in enumerate(clusters):
                if c is not None:
                    medoid2_x,medoid2_y = calculate_medoid(c)
                    #centroid2_x, centroid2_y = calculate_centroid2(c)
                    d = calculate_distance([medoid_x, medoid_y], [medoid2_x, medoid2_y])
                    distances[index] = d
        else: #musí byť centroid
            for index, c in enumerate(clusters):
                if c is not None:
                    centroid2_x, centroid2_y = calculate_centroid2(c)
                    d = calculate_distance([centroid_x, centroid_y], [centroid2_x, centroid2_y])
                    distances[index] = d

        # end = time.time()
        # print("Pocitanie riadka: ", end - start, "s")

        # start = time.time()

        # upraviť riadok v maske

        distances[distances == 0] = np.inf
        matica_vzd[i1] = distances

        # end = time.time()
        # print("Pridanie riadka: ", end - start, "s")

        # print("\n")
        end2 = time.time()
        #print("Time elapsed: ", end2 - start2, "s")

    end = time.time()
    print("Time elapsed: ", end - start, "s")

    plot_clusters(clusters,mode=mode)
    return clusters


















def plot_clusters(clusters, mode = 'centroid'):
    plt.figure(figsize=(10, 10))

    for cluster in clusters:
        if cluster is not None:
            # Náhodná farba pre každý cluster
            color = f'#{random.randrange(256 ** 3):06x}'

            # Ak má cluster viac bodov
            if len(cluster) > 1:
                for point in cluster:
                    # Získanie a transformácia súradníc
                    x, y = point[0], point[1]
                    # Pridanie bodu do scatter plotu
                    plt.scatter(x, y, color=color, s=20)  # s=10 je veľkosť bodu


                    if mode == 'medoid':
                        medoid = calculate_medoid(cluster)
                        plt.scatter(medoid[0], medoid[1], s=15, color="blue", edgecolor='black', linewidth=2)
                    else:
                        centroid = calculate_centroid2(cluster)
                        plt.scatter(centroid[0], centroid[1], s=15, color="red", edgecolor='black', linewidth=2)


                    #plt.plot(x,y, color=color, linestyle='-', linewidth=0.5)
            # Ak má cluster iba jeden bod
            else:
                x, y = cluster[0][0], cluster[0][1]
                #plt.scatter(x, y, color=color, s=10)
                plt.scatter(x, y, s=10, color=color, edgecolor='black', linewidth=2)
                #plt.text(x, y, str(i), color="red", fontsize=12)


    plt.xlabel("X Axis")
    plt.ylabel("Y Axis")
    plt.title("Scatter plot with random colors per cluster")
    plt.show()

    #plt.savefig('sinus_graph.png')



mask = np.triu(np.ones_like(matica_vzd, dtype=bool), k=1)
mask_diag = np.eye(matica_vzd.shape[0], dtype=bool)
mask = mask | mask_diag

matica_vzd[mask] = np.inf


print("zaciname")
#clusters = [c[0] for c in clusters] #obalim body do vlastnych clusterov


start = time.time()

#t = threading.Thread(target=aglomeratne_centroidne, args=(clusters))
#t.start()

aglomeratne_clustering(clusters,mode='medoid')

cluster = (  ( (1,2),(2,3),(3,4),(8,9),(4,6),(9,3),(1,4)             ), (4,5)                )



#calculate_medoid(clusters)
#find_avg_distance_medoid(clusters)

end = time.time()
print("Time elapsed: ",end-start)

#generacia bodov cca 2.15s
#matica vyplnenie cez numpy funkciu: 82s
#matica vyplnenie cez scipy 2.2s ???????? to je crazy

#najdenie minima s maskou dolneho trojuholnika 16s-25s na 20000 bodov
#najdenie minima s indexami dolneho trojuholnika 8-15s cca na 20000 bodov co je crazy (bez youtube aj 3-5s)
#find avg dist cez numpy

#odstranovanie riadkov a stlpcov, pri np.delete sa kopiruje cela mnozina a to je nemyslitelne pre takú velkost
#a hlavne konverzia medzi np a normal je strasne narocna
#konverzia clusterov na np je takmer instantna, problem je ozaj pri tých gigantických arracy nxn


#to-do optimalizacia hladania minima: pre buducnost by som mohol len raz maskovat horny trojuholnik a nie za kazdym znova a znova, ale mozno nie lebo sa menia riadky
#rozsirujem maticu novou np polom ale mozno ho necham tak ako normalny list a len pri operacii ho prevediem, lebo v nom nebude vela prvkov podla mna alebo az tak moc, ale otestujem cez to aj to






#testy mierne neoptimalizovany append: bez pypy
#1000 - 5s, 6s, 5s
#5000 - 2.8min okolo 3 min asi
#cca 0.3 s na iteraciu pri 20000, odhad cca 1h40min


#medoid
#10000 - eta 22min

#nová idea, keďže z matice je probleme odstraniť aj maskou dokonca, tak budeme nahradzať






