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

print(clusters)









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


#print(clusters)

clusters = np.array(list(clusters))

#tvorba matice
start = time.time()
matica_vzd = np.empty((20020,20020))
#matica_vzd = [[0]*1020]*1020
#differences = clusters[:, np.newaxis, :] - clusters[np.newaxis, :, :]

#distances = np.linalg.norm(clusters[:, np.newaxis] - clusters,axis=2)
distances = squareform(pdist(clusters))

#print(matica_vzd)
def calculate_distance(x1,y1,x2,y2):
    #print(x1,x2,y1,y2)
    #return math.sqrt((x1-x2)**2 + (y1-y2)**2) #vzdialenost c
    return np.linalg.norm(np.array([x2,y2]) - np.array([x1,y1]))

#for index1,i in enumerate(clusters):

    #for index2,j in enumerate(clusters):
        #print(i,j)
        #d = calculate_distance(i[0],i[1],j[0],j[1])
        #print(index1,index2)

        #matica_vzd[index1,index2] = d

#print(type(distances))  # Malo by byť <class 'numpy.ndarray'>
#print(distances.shape)
#print(distances)
#df = pd.DataFrame(distances)
#print(tabulate(df, headers='keys', tablefmt='psql'))

end = time.time()
print("Time elapsed: ", end-start, "s")

df = pd.DataFrame(distances[:500, :500])
print(tabulate(df, headers='keys', tablefmt='psql'))









#generacia bodov cca 2.15s
#matica vyplnenie cez numpy funkciu: 82s
#matica vyplnenie cez scipy 2.2s ???????? to je crazy

