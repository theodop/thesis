import cv2
import numpy
from plotnine import ggplot, aes, geom_line
from plotnine.data import economics

def hole_filling_filter(mat: numpy.ndarray):
    for y in range(mat.shape[0]):
        for x in range(mat.shape[1]-2, 0, -1):
            if mat[y,x] == 0:
                mat[y,x] = mat[y,x+1]
    
    return mat

w = 424
h = 240

maxdepth = 5000

array = numpy.fromfile("C:\\Users\\theod\\source\\repos\\thesis\\r-tests\\hallway1_Depth.raw", numpy.int16)
array = numpy.reshape(array, [h, w])
array[array>maxdepth] = maxdepth
array = cv2.medianBlur(array, 5)
array = hole_filling_filter(array)

midrow = array[int(h/2)]

ggplot(economics) + aes(x="date", y="pop") + geom_line()

a = "b"