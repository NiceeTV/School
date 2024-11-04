import numpy as np
import random

c0 = np.empty((1, 20000), dtype=object)

unique_pairs = set()

count = 0
while count != 2000:
    x,y = random.randint(-5000,5000),random.randint(-5000,5000)


    #if any((np.array_equal(elem, [[x,y]]) for elem in c0[0])):
    #c0[0, count] = [[x, y]]
    #count += 1

    #target = [[x,y]]

    #found = np.any([np.array_equal(s[0], [x,y]) for s in c0[0, :] if s is not None])

    #if not found:
    #c0[0, count] = [[x, y]]  # Pridanie novej dvojice
    count += 1
    unique_pairs.add((x, y))
    #else:
        #print("Našlo sa:", [x, y])


array = np.array(list(unique_pairs))

print(array)




