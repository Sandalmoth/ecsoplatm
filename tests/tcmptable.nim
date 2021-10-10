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

  var
    c1, c2: CmpTable[float]

  for i in 1..3:
    c1[i] = i.float
    c2[i + 1] = i.float

  var a = 3.0

  init(Weave)

  syncScope():
    spawn sqr(a.addr)
    # sqr.apply(c1)
  # syncScope():
  #   switch.apply(c1, c2)

  exit(Weave)

  echo a

  echo c1
  echo c2


proc foo(px: ptr int) =
  var x = px[]
  let a = x*x
  let b = x div 177
  px[] = a*a*a - b*b*b


proc doPart(first: ptr UncheckedArray[int], n: int) =
  for i in 0..<n:
    foo(first[i].addr)


test "benchmark":

  let N = 4000000
  var s: seq[int]
  # initialize
  s.setLen(N)
  for i in 0..<N:
    s[i] = i

  var p = cast[ptr UncheckedArray[int]](s[0].unsafeAddr)

  init Weave

  let t0 = getTime()
  for i in 0..<N:
    inc s[i]

  echo s[N div 2], ' ', getTime() - t0

  let t1 = getTime()
  for i in 0..<N:
    spawn foo(s[i].unsafeAddr)
  syncRoot Weave

  echo s[N div 2], ' ', getTime() - t1


  let t2 = getTime()
  parallelFor i in 0..<N:
    captures: {p}
    foo(p[i].addr)
  syncRoot Weave

  echo s[N div 2], ' ', getTime() - t2


  let t3 = getTime()
  for i in 0..<1024:
    let k = N div 1024
    var z = cast[ptr UncheckedArray[int]](s[k*i].unsafeAddr)
    spawn doPart(z, k)
  syncRoot Weave

  echo s[N div 2], ' ', getTime() - t3

  exit Weave






