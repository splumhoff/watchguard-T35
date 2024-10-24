#!/usr/bin/python

from sys import argv, exit

failed=False

fd = open(argv[1])
ref = fd.read()
fd.close()

fd = open(argv[2])
tst = fd.read()
fd.close()

if len(ref) != 512:
  print("Fail: ref is not 512 bytes")
  failed=True

if len(tst) != 512:
  print("Fail: tst is not 512 bytes")
  failed=True

both = zip(range(0, len(tst)+1), ref, tst)

chek = both[0:0x18] + both[0x1c:0x40] + both[0x54:]

f = [(i[0], ord(i[1]), ord(i[2])) for i in chek if i[1] != i[2]]

if len(f) != 0:
  print("Fail: unaccounted differences")
  failed = True
  for item in f:
    print("%3.3x %2.2x %2.2x" % item)

if failed:
  exit(-1)
else:
  print("PASS")
  exit(0)
