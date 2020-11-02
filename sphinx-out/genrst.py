import os
import shutil
from pathlib import Path

index = """
.. StoneMason documentation master file, created by
   sphinx-quickstart on Fri Oct 23 18:22:55 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to StoneMason's documentation!
======================================

.. toctree::
   :maxdepth: 2
   :caption: Contents:



Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`


This is the StoneMason docs. Below is a listing of all the header files and their documentations. To be honest, 
they're just a reformatted version of the header files. You might as well just read the headers lol.


.. toctree::
   :maxdepth: 2
   :caption: STMS Header Structure
   :numbered: 
   
"""


walk_dir = "./include"
output = "./sphinx-out/api"

if os.path.exists(output):
    shutil.rmtree(output)

for root, subdirs, files in os.walk(walk_dir):
    for filename in files:
        file_path = os.path.join(root, filename)[len(walk_dir) + 1:]

        joiner = file_path.split("/")[1:-1]

        if joiner:
            joiner = os.path.join(*joiner)
        else:
            joiner = ""

        to_create = os.path.join(output, joiner)

        Path(to_create).mkdir(parents=True, exist_ok=True)

        out_rst = os.path.join(output, os.path.join(*file_path.split("/")[1:]))

        with open(out_rst[:-len(".hpp")] + ".rst", 'w') as fp:
            fp.write(f"{file_path}\n")
            fp.write("===================\n")
            fp.write(f".. doxygenfile:: {file_path}")

        prefix = os.path.join(*output.split("/")[:-1])
        index += f'   {out_rst[len(prefix) + 1:-len(".hpp")]}.rst\n'


with open("./sphinx-out/index.rst", 'w') as fp:
    fp.write(index)

os.chdir("./sphinx-out")
os.system("make html")
os.chdir("..")

