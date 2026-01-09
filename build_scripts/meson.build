# >>> project definition
project(run_command('sh', '-c', 'echo $GWATCH_TARGET_NAME').stdout().strip(), ['CPP'],
	# Get version number from file.
	version: run_command('sh', '-c', 'echo $GWATCH_VERSION').stdout().strip()
)
project_name = run_command('sh', '-c', 'echo $GWATCH_TARGET_NAME').stdout().strip()

add_project_arguments('-O3',  language : 'cpp')

# >>>>>>>>>>>>>> build prepare >>>>>>>>>>>>>>
# compile flags
c_args = []

# load flags
ld_args = []

# dependencies
deps = []

# include directories
inc_dirs = []

# all libraries that want to be statically linked
libraries = []

# source files
sources = []

# current directory of this meson file
meson_file_dir = meson.current_source_dir()


# >>>>>>>>>>>>>> setup configurations >>>>>>>>>>>>>>
sources += run_command('sh', '-c', 'echo $GWATCH_SRC').stdout().strip().split(',')
if sources.length() == 0
    assert(false, 'no source files were set')
endif

inc_dirs += run_command('sh', '-c', 'echo $GWATCH_INC').stdout().strip().split(',')
if inc_dirs.length() == 0
    assert(false, 'no include directories were set')
endif

ld_args += run_command('sh', '-c', 'echo $GWATCH_LDFLAGS').stdout().strip().split(',')
if ld_args.length() == 0
    assert(false, 'no ld flags were set')
endif

c_args += run_command('sh', '-c', 'echo $GWATCH_CFLAGS').stdout().strip().split(',')
if c_args.length() == 0
    assert(false, 'no c flags were set')
endif

target_type = run_command('sh', '-c', 'echo $GWATCH_TARGET_TYPE').stdout().strip()


# >>>>>>>>>>>>>> start building >>>>>>>>>>>>>>
if target_type == 'exe'
    executable(
        project_name,
        sources,
        dependencies : deps,
        cpp_args : c_args,
        link_args : ld_args,
        include_directories : inc_dirs,
        install : false,
    )
elif target_type == 'lib'
    library(
        project_name,
        sources,
    	dependencies : deps,
    	cpp_args : c_args,
    	link_args : ld_args,
    	include_directories : inc_dirs,
    	install: false
    )
else
    assert(false, 'unknown target type')
endif
