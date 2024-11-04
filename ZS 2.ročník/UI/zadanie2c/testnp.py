import numpy as np
import random
import time




#1. zvol 20 bodov náhodných
clusters = set()
count = 0
while len(clusters) != 3:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)
    clusters.add((x, y))

print(clusters)









count = len(clusters) #toto ešte optimalizovať
start = time.time()
while len(clusters) != 20003:
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

end = time.time()
print("Time elapsed: ", end-start, "s")


"""ä
element_index = random.randrange(0, len(clusters))

    # Obtaining an element from the set based on its index
    # Where the index is obtained randomly
    element = list(clusters)[element_index]
    
    2s zatial najrychlejsie


"""



