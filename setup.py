

from typing import List, Callable, Any, Tuple
from packaging import version
from setuptools import setup, find_packages, Command, Distribution

from build_scripts import *

class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True

setup(
    name='gwatch',
    version=open('VERSION').read().strip(),
    author='Zhuobin Huang',
    author_email='zhuobin@u.nus.edu',
    description='G-Watch is a toolbox for GPU profiling and program analysis.',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    url='https://github.com/G-Watch/G-Watch',
    packages=find_packages(),
    # package_data={
    #     'gwatch': ['*.so', 'libs/*.so', 'bin/*']
    # }, 
    include_package_data=True,
    zip_safe=False,
    ext_modules=[],
    cmdclass={
        'build':    BuildCommand,
        'clean':    CleanCommand,
        'install':  InstallCommand,
    },
    classifiers=[
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.7',
    distclass=BinaryDistribution,
    options={
        'bdist_wheel': {
            'plat_name': 'manylinux2014_x86_64'
        }
    },
    data_files=[]   # this field will be redispatched in install command
)
