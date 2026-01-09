

from typing import List, Callable, Any, Tuple
from packaging import version
from setuptools import setup, find_packages, Command

from build_scripts import *

setup(
    name='gwatch',
    version='0.0.0',
    author='Zhuobin Huang',
    author_email='zhuobin@u.nus.edu',
    description='G-Watch is a profiling toolkit for nVIDIA and AMD GPUs',
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
    data_files=[]   # this field will be redispatched in install command
)
