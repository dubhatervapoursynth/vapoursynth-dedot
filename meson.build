project('Dedot', 'cpp',
    default_options: ['buildtype=release', 'warning_level=2', 'b_lto=true', 'b_ndebug=if-release', 'cpp_std=c++17'],
    license: 'GPL-2.0-only',
    license_files: 'gpl2.txt',
    meson_version: '>=1.2.3',
    version: '3.0',
)
sources = [
  'src/dedot.cpp',
]

incdir = include_directories(
    run_command(
        find_program('python', 'python3'),
        '-c',
        'import vapoursynth as vs; print(vs.get_include())',
        check: true,
    ).stdout().strip()
)

cflags = [
]

host_cpu_family = host_machine.cpu_family()

if host_cpu_family.startswith('x86')
  cflags += ['-mfpmath=sse', '-msse2', '-DDEDOT_X86=1']

  if host_cpu_family == 'x86'
    cflags += '-DDEDOT_32_BITS=1'
  else
    cflags += '-DDEDOT_64_BITS=1'
  endif
endif


cxx = meson.get_compiler('cpp')




py = import('python').find_installation(pure: false)

shared_module('dedot',
    sources,
    gnu_symbol_visibility: 'hidden',
    include_directories: incdir,
    cpp_args: cflags,
    install: true,
    install_dir: py.get_install_dir() / 'vapoursynth/plugins',
    name_prefix: '',
)
