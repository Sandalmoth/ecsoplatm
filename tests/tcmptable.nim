import unittest

import weave

import ecsoplatm


test "always passes":
  check 1 == 1


test "cmptable"
  init(Weave)
  echo "yo"
  exit(Weave)
  # Weave.init()
  # Weave.exit()
