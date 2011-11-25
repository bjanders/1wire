#!/usr/bin/env python

from distutils.core import setup, Extension

owusb = Extension('owusb',
                  libraries = ['usb'],
                  sources = ['ds2490.c', 'owmodule.c'])

setup (name = '1-Wire',
       version = '1.0',
       description = 'Interface to Dallas Semiconductor 1-wire',
       author = 'Bjorn Andersson',
       author_email = 'bjorn@iki.fi',
       packages = ['ow'],
       ext_modules = [owusb])
