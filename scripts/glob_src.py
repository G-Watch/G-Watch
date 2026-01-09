import glob

# add all local source files
sources = glob.glob("./src/**/*.c", recursive=True) + glob.glob("./src/**/*.cpp", recursive=True) 

for i in sources:
    print(i)
