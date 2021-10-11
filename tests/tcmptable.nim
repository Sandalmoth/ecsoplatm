import unittest

import times

import weave

import ecsoplatm/cmptable


test "always passes":
  check 1 == 1


proc sqr(x: ptr float) =
  x[] *= x[]

proc switch(x, y: ptr float) =
  swap(x[], y[])


test "cmptable":

  echo sqr.type
  echo switch.type

  var
    c1, c2: CmpTable[float]

  for i in 1..3:
    c1[i] = i.float
    c2[i + 1] = i.float

  init(Weave)

  syncScope():
    sqr.apply(c1)
  syncScope():
    switch.apply(c1, c2)

  exit(Weave)

  echo c1
  echo c2

