library(hexView)
rows = 240
cols = 424

actualImage <- function(m, ...) {
  image(t(m)[,nrow(m):1], col = gray.colors(32), ...)
}

medianFilter <- function(m, n=5) {
  ndiff <- floor(n/2)
  m2 <- m
  
  for (x in ndiff+1:cols-ndiff) {
    for (y in ndiff+1:rows-ndiff) {
      m2[y,x] <- median(m[y-ndiff:y+ndiff, x-ndiff:x+ndiff])
    }
  }
  return(m2)
}

cap <- 10

d <- readRaw("hallway1_Depth.raw", machine = "hex", human = "int", size=2, signed = FALSE)
m <- matrix(d$fileNum, rows, cols, byrow = TRUE) / 1000

m <- medianFilter(m)

midrow <- m[rows/2,]

thetas <- seq(-87/2,82/2,length.out = cols)
x <- sin(thetas*pi/180) * midrow
y <- cos(thetas*pi/180) * midrow

x[x>cap] <- cap
y[y>cap] <- cap

plot(x,y)

