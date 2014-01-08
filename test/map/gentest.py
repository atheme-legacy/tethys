#!/usr/bin/env python2

import sys

if len(sys.argv) < 2:
    print('missing test name argument')
    print('will gen $1 and $1.out')
    sys.exit(1)

INPUT = open(sys.argv[1], 'w')
OUTPUT = open(sys.argv[1] + '.out', 'w')

def to_input(s):
    print(s)
    INPUT.write(s + '\n')
def to_output(s):
    OUTPUT.write(s + '\n')

def gen_add(k, v):
    my_map[k] = v
    to_input('+{}={}'.format(k, v))

def gen_debug():
    to_input('*')

def gen_dump():
    to_input('d')
    for k in sorted(my_map.keys()):
        to_output('{}={}'.format(k, my_map[k]))

def gen_del(k):
    to_input('-{}'.format(k))
    to_output('{}'.format(my_map[k]))
    del my_map[k]

def gen_quit():
    to_input('q')
    to_output('bye')

import random
import string

def name(n=10):
    return ''.join(random.choice(string.letters) for x in range(n))

my_map = {}

def n_adds(n):
    for x in range(n):
        s = name()
        gen_add(s, s)
def n_dels(n):
    for x in range(n):
        gen_del(random.choice(my_map.keys()))

n_adds(1000)
n_dels(999)
gen_dump()

gen_quit()

INPUT.close()
OUTPUT.close()
