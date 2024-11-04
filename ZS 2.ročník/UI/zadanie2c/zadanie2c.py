import tkinter
import random
import math
import numpy as np
from scipy.spatial.distance import euclidean
import matplotlib.pyplot as plt
import pandas as pd
from tabulate import tabulate
from scipy.spatial.distance import pdist, squareform


#myšlienka bude 2-rozmerná množina clusterov, pri tom centroid based
#generovanie
#X,Y = sú od -5000 do 5000

matica_vzdialenosti = []


#1. zvol 20 bodov náhodných
clusters = set()
while len(clusters) != 20:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)
    clusters.add((x, y))

print(clusters)

#dalsich 20000 bodov
#toto ešte optimalizovať
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


print(clusters)


def calculate_distance(x1,y1,x2,y2):
    #print(x1,x2,y1,y2)
    return math.sqrt((x1-x2)**2 + (y1-y2)**2) #vzdialenost c

#vyplnit maticu vzdialenosti
clusters = np.array(list(clusters))

#tvorba matice
matica_vzd = np.empty((20020,20020)) #inicializacia matice vzdialenosti
distances = squareform(pdist(clusters)) #výpočet vzdialeností, pdist vypočíta vzdialenost raz pre každú dvojicu a squareform vloží do matice

def calculate_distance(x1,y1,x2,y2): #optimalizovaný vzorec na vzdialenost v numpy
    return np.linalg.norm(np.array([x2,y2]) - np.array([x1,y1]))


def calculate_centroid(body): #funkcia s numpy v C optimalizovaná
    centroid = np.mean(body, axis=0) #vypočíta priemer súradníc pola
    c = (int(centroid[0]), int(centroid[1])) #premena na int, lebo máme pixely a potrebujeme celé súradnice
    return c



"""def find_min(matica):
    n = len(matica)
    minimum = float('inf')  # Inicializácia na nekonečno
    row_min, col_min = -1, -1  # Inicializácia indexov

    # Prechádzame iba horný trojuholník matice (kde j > i)
    for i in range(1,n):
        for j in range(i):
            if matica[i][j] < minimum and matica[i][j] != 0.0:
                minimum = matica[i][j]
                row_min, col_min = i, j

    print("Minimum:", minimum, "at position (", row_min, ",", col_min, ")")
    return minimum, row_min, col_min"""

def find_min(matica):
    minimum = np.min(matica) #nájde v celej matici najnižšiu hodnotu
    index1,index2 = np.unravel_index(minimum,(matica.shape[0],matica.shape[1]))







def find_avg_distance(c):
    print("\n# rátam avg #\n")
    centroid_x, centroid_y = calculate_centroid(c)

    vzdialenosti = 0

    for body in c:
        d = calculate_distance(centroid_x,centroid_y,body[0],body[1])
        print("d: ",centroid_x,centroid_y,body[0],body[1])
        vzdialenosti += d
        print("added: ",body, " d: ", d)

    avg = vzdialenosti/len(c)
    print("centroid: ", centroid_x, centroid_y)
    print("vzd: ",vzdialenosti, " poc: ", len(c))
    print("\n# koniec avg #\n")


    if avg <= 500:
        print("cluster je ok")
        return 0
    else:
        print("cluster je nie ok ", avg)
        return 1


def remove_column(matica, col_idx):
    for row in matica:
        if col_idx < len(row):
            row.pop(col_idx)
    return matica


def remove_indexes(pole,i1,i2):
    # nech i1 je mensie
    if i1 > i2:
        i1, i2 = i2, i1
    pole.pop(i1)

    if len(pole) > 0:
        pole.pop(i2-1)

    return pole


def remove_indexes_vzd(pole,i1,i2):
    #nech i1 je mensie
    if i1 > i2:
        i1,i2 = i2,i1
    pole.pop(i1)

    pole = remove_column(pole,i1)

    #if len(pole) > 0:
    pole.pop(i2-1)
    pole = remove_column(pole,i2-1)



    return pole



def aglomeratne_centroidne(clusters):
    global matica_vzdialenosti

    while True:
        print("pocet: ",len(clusters))
        df = pd.DataFrame(matica_vzdialenosti)
        print(tabulate(df, headers='keys', tablefmt='psql'))
        najmensia_vzd,i1,i2 = find_min(matica_vzdialenosti)

        #zlucenie clusterov
        c1 = clusters[i1]
        c2 = clusters[i2]
        new_cluster = c1 + c2



        #check noveho clustera
        podmienka = find_avg_distance(new_cluster)
        if podmienka == 1 or len(clusters) == 1:
            break


        #[clusters[i1],clusters[i2]]

        #print("1: ",c1, "2: ",c2, " vysledok: ", new_cluster)


        #clusters.remove(c1) #odstranim c2
        #clusters.remove(c2)
        clusters = remove_indexes(clusters,i1,i2)


        #print("new c1: ",clusters[i1])

        #clusters[i1] = new_cluster #nahradim c1 s novym clusterom

        #vymaz riadky i1,i2
        #pridaj riadok na koniec s clusterom a zapln ho hodnotami

        #to iste v clusters

        matica_vzdialenosti = remove_indexes_vzd(matica_vzdialenosti, i1, i2)

        centroid_x, centroid_y = calculate_centroid(new_cluster)

        riadok = []
        for i in clusters:
            print("s kolkymi? ", len(clusters))
            centroid2_x, centroid2_y = calculate_centroid(i)
            d = calculate_distance(centroid_x, centroid_y, centroid2_x, centroid2_y)
            riadok.append(d)




        #matica_vzdialenosti = remove_column(matica_vzdialenosti,i1)
        #matica_vzdialenosti = remove_column(matica_vzdialenosti, i2)

        #matica_vzdialenosti.remove(matica_vzdialenosti[i1])
        #matica_vzdialenosti.remove(matica_vzdialenosti[i2])
        #print("l: ",len(riadok), riadok)
        matica_vzdialenosti.append(riadok)

        clusters.append(new_cluster)  # na koniec

        #print("matica vzd: ",len(matica_vzdialenosti), " cluster vzd: ",len(clusters))
        #print("end: ",matica_vzdialenosti[0][-1])


aglomeratne_centroidne(clusters)

#print(clusters)
#print(matica_vzdialenosti)

"""print("pocet clusterov: ",len(clusters))
for i in clusters:
    color = f'#{random.randrange(256 ** 3):06x}'  # pre každý cluster zvlášť farba
    if len(i) > 1:
        #print(i)
        for j in i:
            #print(j[0],j[1])
            d = 5000 / 400
            x, y = (j[0]) / d + 400, (j[1]) / d + 400
            #x,y = (j[0] +5000),(j[1]+5000)
            #print(x,y)
            canvas.create_oval(x-5,y-5,x+5,y+5,fill=color)

    else:
        d = 5000 / 400
        x, y = (i[0][0]) / d + 400, (i[0][1]) / d + 400
        # x,y = (i[0] + 5000),(i[1]+5000)
        # print(i[0])
        canvas.create_oval(x-5, y-5, x+5, y+5, fill=color)"""


def plot_clusters(clusters):
    # Mierka podľa poskytnutého kódu


    # Vytvorenie scatter plotu
    plt.figure(figsize=(10, 10))

    # Iterácia cez každý cluster
    for cluster in clusters:
        # Náhodná farba pre každý cluster
        color = f'#{random.randrange(256 ** 3):06x}'

        # Ak má cluster viac bodov
        if len(cluster) > 1:
            for point in cluster:
                # Získanie a transformácia súradníc
                x, y = point[0], point[1]
                # Pridanie bodu do scatter plotu
                plt.scatter(x, y, color=color, s=10)  # s=10 je veľkosť bodu

        # Ak má cluster iba jeden bod
        else:
            x, y = cluster[0][0], cluster[0][1]
            plt.scatter(x, y, color=color, s=10)

    # Zobrazenie grafu
    #plt.xlim(0, 800)  # Nastavenie limitov na osi X podľa posunu
    #plt.ylim(0, 800)  # Nastavenie limitov na osi Y podľa posunu
    plt.xlabel("X Axis")
    plt.ylabel("Y Axis")
    plt.title("Scatter plot with random colors per cluster")
    plt.show()


    #canvas.create_oval(x - 5, y - 5, x + 5, y + 5, fill="black")
    #print(i)

    #if len(i) > 1:
        #print(i)
        #for j in i:
            #print(j[0],j[1])
            #x,y = (j[0] +5000),(j[1]+5000)
            #print(x,y)
            #canvas.create_oval(x-5,y-5,x+5,y+5,fill=color)

    #else:
    #x,y = (i[0] + 5000),(i[1]+5000)
    #print(i[0])
    #canvas.create_oval(x-5, y-5, x+5, y+5, fill=color)


#plot_clusters(clusters)

#canvas.mainloop()





#stop podmienka je ak priemerna vzdialenost clusterov je viac ako 500, že ak by som náhodol spojil clustere ktoré by mali viac ako 500











