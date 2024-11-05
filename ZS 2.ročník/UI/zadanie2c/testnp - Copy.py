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
np.random.seed = 42

while len(clusters) != 20:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)
    clusters.add((x, y))




count = len(clusters) #toto ešte optimalizovať
while len(clusters) != 20020:
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


#df = pd.DataFrame(matica_vzd[:100, :100])
#print(tabulate(df, headers='keys', tablefmt='psql'))


#cluster = (  ( (1,2),(2,3),(3,4),(8,9),(4,6),(9,3),(1,4)             ), (4,5)                )
#print(cluster[0])


def calculate_centroid2(body):
    if len(body) == 1:
        return body[0]
    else:
        centroid = np.mean(body, axis=0)
        return centroid


def update_mask(mask,i1,i2):
    #aktualizujeme masku namiesto toho, aby sme robili novu masku na takmer identickú maticu
    mask[i2, :] = True  #maskovanie riadkov
    mask[:, [i1,i2]] = True  #maskovanie stlpcov
    return mask



def find_min(matica):
    #print(len(matica))
    if matica.size > 1:
        dolny_troj_indexy = np.tril_indices(matica.shape[0], -1) #vyberie indexy hodnot dolneho trojuholnika
        dolny_troj_values = matica[dolny_troj_indexy] #extrahuje hodnoty dolneho trojuholnika

        valid_mask = (dolny_troj_values != 0) & (~np.isnan(dolny_troj_values))  #maska pre hodnoty, ktoré nie su 0 ani nan
        valid_values = dolny_troj_values[valid_mask]  #extrahovanie hodnot podla masky/filtra

        minimum = np.min(valid_values) #nájdenie minima v platných hodnotách

        valid_indices = np.where(valid_mask)[0] #platné indexy
        min_valid = valid_indices[np.argmin(valid_values)] #platný index prcej pozície minima v maskovanej matici
        min_index = (dolny_troj_indexy[0][min_valid], dolny_troj_indexy[1][min_valid]) #prevod na indexy 2d matice
        #m = np.min(matica_vzd)
        #i, j = np.unravel_index(np.argmin(matica_vzd), matica_vzd.shape)

        print(minimum, min_index)
        #print(clusters[i],clusters[j])
        #print("d: ",matica_vzd[i,j])
        return minimum, min_index[0], min_index[1]
    else:
        return -1, -1, -1

def find_min2(matica):
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
    return minimum, row_min, col_min



def find_min_idx(x):



    #k = x.argmin()
    #ncol = x.shape[1]
    #minimum,i1,i2 = x[k//ncol,k%ncol], k//ncol,k%ncol
    #i1,i2 = k//ncol,k%ncol
    #minimum = x[i1,i2]
    i, j = np.unravel_index(np.argmin(matica_vzd), matica_vzd.shape)
    minimum = x[i,j]
    print(minimum,i,j)

    #print(clusters[i1],clusters[i2])

    return minimum, i, j




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

    #print("centroid: ", centroid)

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


def remove_indexes_c(pole,i2):
    np_pole = np.array(pole,dtype=object)

    mask = np.ones(np_pole.shape[0], dtype=bool)
    mask[i2] = False  # Nastavenie hodnôt na False pre indexy, ktoré chceme odstrániť

    # Aplikovanie masky
    filtered_pole = np_pole[mask]
    return filtered_pole.tolist()


def remove_indexes_vzd(pole,i1,i2): #odstranenie riadkov aj stlpcov pre matica_vzd
    riadky, stlpce = pole.shape

    # Vytvorenie masky, ktorá všetko zachová
    mask = np.ones((riadky, stlpce), dtype=bool)

    # Nastavenie hodnôt na False pre riadky a stĺpce, ktoré chceš odstrániť
    mask[[i1,i2], :] = False #maskovanie riadkov
    mask[:, [i1,i2]] = False #maskovanie stlpcov

    filtered_pole = pole[mask].reshape((riadky - 2, stlpce - 2))  # Zmena tvaru matice
    return filtered_pole

def remove_indexes_vzd2(pole,i1,i2):
    mask = np.ones(pole.shape[0], dtype=bool)
    mask[[i1, i2]] = False  # Set to False for the indices we want to remove

    filtered_pole = pole[mask]  # Filter rows
    filtered_pole = filtered_pole[:, mask]  # Filter columns
    return filtered_pole















def aglomeratne_centroidne(clusters,mask):
    global matica_vzd

    #np.fill_diagonal(matica_vzd, np.inf)

    #mask = np.triu(np.ones_like(matica_vzd, dtype=bool), k=1)
    # Nastaviť horný trojuholník na nekonečno
    #matica_vzd[mask] = np.inf


    while True:
        pocet = len([x for x in clusters if x is not None])
        print("\npocet: ",pocet)


        #df = pd.DataFrame(matica_vzd)
        #print(tabulate(df, headers='keys', tablefmt='psql'))


        start = time.time()

        najmensia_vzd,i1,i2 = find_min_idx(matica_vzd)


        end = time.time()
        print("Najdenie minima: ", end - start, "s")


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

        #clusters = remove_indexes(clusters,i1,i2)

        #start = time.time()




        #clusters = remove_indexes_c(clusters,i2)
        #del clusters[i2]
        clusters[i2] = None

        clusters[i1] = new_cluster

        #print("new c1: ",clusters[i1])

        #end = time.time()
        #print("Remove clusters idx + odstranenie z clusters: ", end - start, "s")




        #start = time.time()

        #matica_vzd = remove_indexes_vzd(matica_vzd,i1,i2)



        mask = update_mask(mask,i1,i2) #zamaskujem stlpce a riadky i2
        matica_vzd = np.ma.masked_array(matica_vzd, mask=mask)







        #matica_vzd = np.ma.masked_array(matica_vzd, mask=mask)



        #end = time.time()
        #print("Remove z matice_vzd: ", end - start, "s")

        centroid_x, centroid_y = calculate_centroid2(new_cluster)



        #start = time.time()
        distances = np.zeros(matica_vzd.shape[0])


        """centroids = np.array([calculate_centroid2(cluster) for cluster in clusters])

        for index, (centroid2_x, centroid2_y) in enumerate(centroids):
            d = calculate_distance([centroid_x, centroid_y], [centroid2_x, centroid2_y])
            distances[index] = d"""


        for index,c in enumerate(clusters):
            if c is not None:
                centroid2_x, centroid2_y = calculate_centroid2(c)
                d = calculate_distance([centroid_x, centroid_y], [centroid2_x, centroid2_y])
                distances[index] = d





        #distances[-1] = np.nan

        #end = time.time()
        #print("Pocitanie riadka: ", end - start, "s")

        #distances[i2] = 0
        #distances[i1] = 0

        #start = time.time()

        #nahradíme i1 novým riadkom


        #upraviť riadok v maske
        mask[i1] = False
        mask = np.logical_or(mask,distances == 0)

        distances[distances == 0] = np.nan

        matica_vzd[i1] = distances

        matica_vzd = np.ma.masked_array(matica_vzd, mask=mask)



        end = time.time()
        print("Time elapsed: ", end - start, "s")

        print("\n")
        #end = time.time()
        #print("Time elapsed: ", end - start, "s")

        #df = pd.DataFrame(matica_vzd[:120, :120])
        #print(tabulate(df, headers='keys', tablefmt='psql'))



        #print(clusters)
    plot_clusters(clusters)
    return clusters



def plot_clusters(clusters):
    # Mierka podľa poskytnutého kódu


    # Vytvorenie scatter plotu
    plt.figure(figsize=(10, 10))

    # Iterácia cez každý cluster
    i = 0
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
                    plt.scatter(x, y, color=color, s=10)  # s=10 je veľkosť bodu
                    #plt.text(x, y, str(i), color="red", fontsize=6)
                    i += 1

                    #plt.plot(x,y, color=color, linestyle='-', linewidth=0.5)
            # Ak má cluster iba jeden bod
            else:
                x, y = cluster[0][0], cluster[0][1]
                plt.scatter(x, y, color=color, s=10)
                #plt.text(x, y, str(i), color="red", fontsize=12)
                i += 1

    # Zobrazenie grafu
    #plt.xlim(0, 800)  # Nastavenie limitov na osi X podľa posunu
    #plt.ylim(0, 800)  # Nastavenie limitov na osi Y podľa posunu
    plt.xlabel("X Axis")
    plt.ylabel("Y Axis")
    plt.title("Scatter plot with random colors per cluster")
    plt.show()

    #plt.savefig('sinus_graph.png')


def plot2(clusters):
    # Mierka podľa poskytnutého kódu

    # Vytvorenie scatter plotu
    plt.figure(figsize=(10, 10))

    # Iterácia cez každý cluster
    i = 0
    for cluster in clusters:
        # Náhodná farba pre každý cluster
        print("vykreslujem tento cluster: ",cluster)
        color = f'#{random.randrange(256 ** 3):06x}'

        # Ak má cluster viac bodov
        if len(cluster) > 1:
            for point in cluster:
                # Získanie a transformácia súradníc
                x, y = point[0], point[1]
                # Pridanie bodu do scatter plotu
                plt.scatter(x, y, color=color, s=10)  # s=10 je veľkosť bodu
                #plt.text(x, y, str(i), color="red", fontsize=6)
                #i += 1

                #plt.plot(x, y, color=color, linestyle='-', linewidth=0.5)
        # Ak má cluster iba jeden bod
        else:
            x, y = cluster[0][0], cluster[0][1]
            plt.scatter(x, y, color=color, s=10)
            #plt.text(x, y, str(i), color="red", fontsize=12)
            #i += 1

    # Zobrazenie grafu
    # plt.xlim(0, 800)  # Nastavenie limitov na osi X podľa posunu
    # plt.ylim(0, 800)  # Nastavenie limitov na osi Y podľa posunu
    plt.xlabel("X Axis")
    plt.ylabel("Y Axis")
    plt.title("Scatter plot with random colors per cluster")
    plt.show()








#clusters = remove_indexes(clusters,2,4)
#clusters = remove_indexes_c(clusters,2,4)


#minimum, i1, i2 = find_min(matica_vzd)

#find_min(matica_vzd)
#minimum,i1,i2 = find_min(matica_vzd)

#find_avg_distance(cluster[0])
#find_avg_distance2(cluster[0])


#matica_vzd = remove_indexes_vzd(matica_vzd,2,4)

#df = pd.DataFrame(matica_vzd[:120, :120])
#print(tabulate(df, headers='keys', tablefmt='psql'))
print("zaciname")
start = time.time()

#df = pd.DataFrame(matica_vzd[:120, :120])
#print(tabulate(df, headers='keys', tablefmt='psql'))

#np.fill_diagonal(matica_vzd, np.inf)

#t = threading.Thread(target=aglomeratne_centroidne, args=(clusters,))
#t.start()


np.fill_diagonal(matica_vzd, np.inf)
mask = np.triu(np.ones_like(matica_vzd, dtype=bool), k=1)

mask_diag = np.eye(matica_vzd.shape[0], dtype=bool)

mask = mask | mask_diag

matica_vzd = np.ma.masked_array(matica_vzd, mask=mask)

#valid_mask = np.ones(matica_vzd, dtype=bool)

#np.fill_diagonal(matica_vzd, np.inf)

t = threading.Thread(target=aglomeratne_centroidne, args=(clusters,mask))
t.start()

#clusters = aglomeratne_centroidne(clusters)

#matica_vzd2 = np.array(matica_vzd)

#print(np.array_equal(matica_vzd,matica_vzd2))


#minimum,i1,i2 = find_min_idx(matica_vzd)
#print(clusters[i1],clusters[i2])

#np.fill_diagonal(matica_vzd, np.inf)

#m = np.min(matica_vzd)
#i, j = np.unravel_index(np.argmin(matica_vzd), matica_vzd.shape)

#print(i,j,m)

end = time.time()
print("Time elapsed: ", end-start, "s")

df = pd.DataFrame(matica_vzd[:120, :120])
print(tabulate(df, headers='keys', tablefmt='psql'))

#plot_clusters(clusters)

#minimum, i1,i2 = find_min(matica_vzd,mask)
#min2,i3,i4 = find_min2(matica_vzd)


#druhé kolo
#úprava masky











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






#testy mierne neoptimalizovany append:
#1000 - 8.4s, 8.39s
#5000




#nová idea, keďže z matice je probleme odstraniť aj maskou dokonca, tak budeme nahradzať






