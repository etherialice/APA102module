from distutils.core import setup, Extension

apa102module = Extension('apa102',
					sources = ['apa102module.c'])

setup (name = 'apa102',
		version = '1.0',
		description = 'APA102 Driver',
		author='Spencer Tinnin',
		url='https://github.com/etherialice/APA102module',
		ext_modules = [apa102module])