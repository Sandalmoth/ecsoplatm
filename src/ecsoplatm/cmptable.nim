import weave


type
  CmpTable*[T] = object
    data: seq[(int, T)]


proc lowerBound[T](t: CmpTable[T], key: int): int =
  ## return the position where inserting would produce
  ## a sorted list
  let len = t.data.len
  if len == 0:
    return 0
  if len == 1:
    if key <= t.data[0][0]:
      return 0
    else:
      return 1

  var
    x = len
  while result < x:
    var mid = (result + x) shr 1
    let y = t.data[mid][0]
    if y == key:
      return mid
    elif y < key:
      result = mid + 1
    else:
      x = mid


proc `[]=`*[T](t: var CmpTable[T], key: int, val: sink T) =
  ## Inserts a ``(key, val)`` pair into ``t``.
  ## If ``key`` is present, change associated ``val``.
  let lb = t.lowerBound(key)
  if lb == t.data.len:
    t.data.add((key, val))
    return
  if t.data[lb][0] == key:
    # key is present, change value
    t.data[lb][1] = val
  else:
    t.data.insert((key, val), lb)


proc `[]`*[T](t: CmpTable[T], key: int): lent T =
  ## Retrieve value for ``key``.
  ## Raises ``KeyError`` if key is not present
  if t.data.len == 0:
    raise new KeyError
  let lb = t.lowerBound(key)
  if t.data[lb][0] == key:
    result = t.data[lb][1]
  else:
    raise new KeyError


proc `[]`*[T](t: var CmpTable[T], key: int): var T =
  ## Retrieve value for ``key``.
  ## Raises ``KeyError`` if key is not present.
  if t.data.len == 0:
    raise new KeyError
  let lb = t.lowerBound(key)
  if t.data[lb][0] == key:
    result = t.data[lb][1]
  else:
    raise new KeyError


# proc getOrNil*[T](t: var CmpTable[T], key: int): ptr T =
#   ## Retrieve pointer to value associated with ``key``.
#   ## Returns ``nil`` if key is not present.
#   if t.data.len == 0:
#     return nil
#   let lb = t.lowerBound(key)
#   if t.data[lb][0] == key:
#     result = t.data[lb][1].addr
#   else:
#     result = nil


# proc getOrDefault*[T](t: CmpTable[T], key: int): (T, bool) =
#   ## Retrieve value associated with ``key``.
#   ## Returns default constructed if key is not present.
#   ## Second tuple element is true if the key was found
#   if t.data.len == 0:
#     return
#   let lb = t.lowerBound(key)
#   if t.data[lb][0] == key:
#     result = (t.data[lb][1], true)


proc hasKey*[T](t: CmpTable[T], key: int): bool =
  ## Test whether ``key`` is present.
  let lb = t.lowerBound(key) 
  lb < t.data.len and t.data[lb][0] == key


proc contains*[T](t: CmpTable[T], key: int): bool =
  ## Alias of `hasKey proc<#hasKey,CmpTable[T],T>`_ for use with ``in``.
  return t.hasKey(key)


proc len*[T](t: CmpTable[T]): int =
  t.data.len


# proc del*[T](t: var CmpTable[T], key: int) =
#   ## Delete ``key`` from ``t``. Does nothing if the key is not present.
#   let lb = t.lowerBound(key)
#   if lb < t.len and t.data[lb][0] == key:
#     t.data.delete(lb)


# proc pop*[T](t: var CmpTable[T], key: int, val: var T) =
#   ## Delete ``key`` from ``t``. If a key was removed, ``val`` is set
#   ## to the removed value.
#   let lb = t.lowerBound(key)
#   if lb < t.len and t.data[lb][0] == key:
#     val = t.data[lb][1]
#     t.data.delete(lb)


proc clear*[T](t: var CmpTable[T]) =
  ## remove all items in ``t``.
  t.data.setLen(0)


proc `$`*[T](t: CmpTable[T]): string =
  result = "CmpTable: { "
  for kv in t.data:
    result = result & $kv & " "
  result = result & "}"


# multithreaded apply functions, using weave
# assumes an active weave worker pool
# does not guarantee ordered execution

proc apply*[F, T](f: F, t: var CmpTable[T]) =
  for i in 0..<t.len:
    f(t.data[i][1].addr)


proc apply*[F, T, U](f: F, a: var CmpTable[T], b: var CmpTable[U]) =
  # for i in 0..<t.len:
  #   f(t.data[i][1])

  if a.len > 0 and b.len > 0:
    var ai, bi: int
    while ai < a.data.len and bi < b.data.len:
      let cmpres = cmp(a.data[ai][0], b.data[bi][0])
      if cmpres == 0:
        spawn f(a.data[ai][1].addr, b.data[bi][1].addr)
        inc ai
        inc bi
      elif cmpres < 0:
        inc ai
      else:
        inc bi


# # TODO: In principle, values and pvalues for an arbitrary number of containers
# # could probably be generated using a varargs macro.
# # however, hand implementation may be enough, as high arity versions
# # probably aren't needed.

# iterator values*[T](t: CmpTable[T]): lent T =
#   ## Iterate over all the values in ``t``.
#   for i in 0..<t.len:
#     yield t.data[i][1]


# iterator pairs*[T](t: CmpTable[T]): lent (int, T) =
#   ## Iterate over all the values in ``t``.
#   for i in 0..<t.len:
#     yield t.data[i]


# iterator pvalues*[T](t: var CmpTable[T]): ptr T =
#   ## Iterate over all the values in ``t``, returning a pointer.
#   for i in 0..<t.len:
#     yield t.data[i][1].addr


# iterator ppairs*[T](t: var CmpTable[T]): (int, ptr T) =
#   ## Iterate over all the values in ``t``, returning a pointer.
#   for i in 0..<t.len:
#     yield (t.data[i][0], t.data[i][1].addr)


# iterator values*[T, U](a: CmpTable[T], b: CmpTable[U]): (T, U) =
#   ## Iterate over all values where the same key is present in ``a`` and ``b``.
#   if a.len > 0 and b.len > 0:
#     var ai, bi: int
#     while ai < a.data.len and bi < b.data.len:
#       let cmpres = cmp(a.data[ai][0], b.data[bi][0])
#       if cmpres == 0:
#         yield (a.data[ai][1], b.data[bi][1])
#         inc ai
#         inc bi
#       elif cmpres < 0:
#         inc ai
#       else:
#         inc bi


# iterator pvalues*[T, U](
#   a: var CmpTable[T],
#   b: var CmpTable[U]
# ): (ptr T, ptr U) =
#   ## Return pointer to values for all keys present int ``a`` and ``b``.
#   if a.len > 0 and b.len > 0:
#     var ai, bi: int
#     while ai < a.data.len and bi < b.data.len:
#       let cmpres = cmp(a.data[ai][0], b.data[bi][0])
#       if cmpres == 0:
#         yield (a.data[ai][1].addr, b.data[bi][1].addr)
#         inc ai
#         inc bi
#       elif cmpres < 0:
#         inc ai
#       else:
#         inc bi


# iterator ppairs*[T, U](
#   a: var CmpTable[T],
#   b: var CmpTable[U]
# ): (int, ptr T, ptr U) =
#   ## Return entity id and pointer to values
#   ## for all keys present int ``a`` and ``b``.
#   if a.len > 0 and b.len > 0:
#     var ai, bi: int
#     while ai < a.data.len and bi < b.data.len:
#       let cmpres = cmp(a.data[ai][0], b.data[bi][0])
#       if cmpres == 0:
#         yield (a.data[ai][0], a.data[ai][1].addr, b.data[bi][1].addr)
#         inc ai
#         inc bi
#       elif cmpres < 0:
#         inc ai
#       else:
#         inc bi


# iterator values*[T, U, V](
#   a: CmpTable[T],
#   b: CmpTable[U],
#   c: CmpTable[V]
# ): (T, U, V) =
#   ## Iterate over all values where the same key is present in 
#   ## ``a``, ``b`` and ``c``.
#   if a.len > 0 and b.len > 0 and c.len > 0:
#     var ai, bi, ci: int
#     while ai < a.data.len and bi < b.data.len and ci < c.data.len:
#       let cmp_ab = cmp(a.data[ai][0], b.data[bi][0])
#       let cmp_ac = cmp(a.data[ai][0], c.data[ci][0])
#       let cmp_bc = cmp(b.data[bi][0], c.data[ci][0])
#       if cmp_ab == 0 and cmp_ac == 0:
#         yield (a.data[ai][1], b.data[bi][1], c.data[ci][1])
#         inc ai
#         inc bi
#         inc ci
#       elif cmp_ab < 0 or cmp_ac < 0:
#         inc ai
#       elif cmp_ab > 0 or cmp_bc < 0:
#         inc bi
#       elif cmp_ac > 0 or cmp_bc > 0:
#         inc ci


# iterator pvalues*[T, U, V](
#   a: var CmpTable[T], 
#   b: var CmpTable[U],
#   c: var CmpTable[V]
# ): (ptr T, ptr U, ptr V) =
#   ## Iterate over all values where the same key is present in 
#   ## ``a``, ``b`` and ``c``. Return tuple of pointer to values.
#   if a.len > 0 and b.len > 0 and c.len > 0:
#     var ai, bi, ci: int
#     while ai < a.data.len and bi < b.data.len and ci < c.data.len:
#       let cmp_ab = cmp(a.data[ai][0], b.data[bi][0])
#       let cmp_ac = cmp(a.data[ai][0], c.data[ci][0])
#       let cmp_bc = cmp(b.data[bi][0], c.data[ci][0])
#       if cmp_ab == 0 and cmp_ac == 0:
#         yield (a.data[ai][1].addr, b.data[bi][1].addr, c.data[ci][1].addr)
#         inc ai
#         inc bi
#         inc ci
#       elif cmp_ab < 0 or cmp_ac < 0:
#         inc ai
#       elif cmp_ab > 0 or cmp_bc < 0:
#         inc bi
#       elif cmp_ac > 0 or cmp_bc > 0:
#         inc ci
