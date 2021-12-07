import threadpool


const
  batch_size: int = 2


type
  Entity = uint32

  Component[T] = object
    data: seq[(Entity, T)]
    # TODO create and destroy queues should be appendable in parallel
    create_queue: seq[(Entity, T)]
    destroy_queue: seq[(Entity, int)] # indices into data vector


proc len[T](a: Component[T]): int =
  a.data.len


proc update[T](a: var Component[T]) =
  for c in a.create_queue:
    a.data.add(c)
  a.create_queue.set_len(0)


proc create[T](a: var Component[T], id: Entity, value: sink T) =
  a.create_queue.add((id, value))



# one component

proc apply_impl[T](f: proc (x: ptr T), a: ptr Component[T], first, last: int) =
  echo "single component impl call"
  for i in first..<last:
    f(a.data[i][1].addr)


template apply(f, a: untyped) =
  var i = 0
  while i < a.len:
    spawn apply_impl(f, a.addr, i, min(a.len, i + batch_size))
    i += batch_size


# two components

proc apply_impl[T, U](
  f: proc (x: ptr T, y: ptr U),
  a: ptr Component[T],
  b: ptr Component[U],
  afirst, alast, bfirst, blast: int
                 ) =
  echo "two component impl call"
  var
    ai = afirst
    bi = bfirst

  while ai < alast and bi < blast:
    let cmpres = cmp(a.data[ai][0], b.data[bi][0])
    echo ai, ' ', bi, ' ', cmpres
    if cmpres == 0:
      f(a.data[ai][1].addr, b.data[bi][1].addr)
      inc ai
      inc bi
    elif cmpres < 0:
      inc ai
    else:
      inc bi


template apply(f, a, b: untyped) =
  let n = (a.len + b.len) div (batch_size*2) + 1 # number of work blocks
  if (n == 0): return # no work to do
  elif (n == 1):
    spawn apply_impl(f, a.addr, b.addr, 0, a.len, 0, b.len)
  else:
    # break up work into multiple chunks
    let
      astep = a.len div n
      bstep = b.len div n

    var
      breaks = new_seq_of_cap[Entity](n)
    for i in 1..<n:
      breaks.add( (a.data[i*astep][0] + b.data[i*bstep][0]) div 2 )

    echo breaks


# testing

proc sqr(x: ptr int) =
  x[] *= x[]


proc add_to_left(x, y: ptr int) =
  x[] += y[]


proc main() =
  var a, b: Component[int]

  a.create(1.uint32, 1)
  a.create(2.uint32, 2)
  a.create(3.uint32, 3)
  b.create(2.uint32, 2)
  b.create(3.uint32, 3)
  b.create(4.uint32, 4)

  # TODO replace with single function generate by macro or something
  a.update()
  b.update()

  sqr.apply(a)
  sqr.apply(b)
  sync()
  add_to_left.apply(b, a)
  sync()

  echo a.data
  echo b.data


main()
