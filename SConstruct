env=Environment(CFLAGS = ['-std=c99','-g'])

LIB_SRC_FILES=[
	'./capture.c',
	'./control.c',
	'./frame.c',
	'./version.c',
	]

INCLUDED_PATH=['.','/usr/include/glib-2.0' ,'/usr/include/', '/usr/lib/x86_64-linux-gnu/glib-2.0/include']
INCLUDED_LIBPATH=['.','/usr/local/lib']

DEPENDED_LIBS2=['fg2','v4l2','v4lconvert', 'SDL2','glib-2.0','pthread'] 

env.StaticLibrary('fg2', LIB_SRC_FILES, CPPPATH=INCLUDED_PATH)

env.Program('cam_capture_store_example.c', LIBS=DEPENDED_LIBS2, LIBPATH=INCLUDED_LIBPATH, CPPPATH=INCLUDED_PATH)
env.Program('cam_capture_demon.c', LIBS=DEPENDED_LIBS2, LIBPATH=INCLUDED_LIBPATH, CPPPATH=INCLUDED_PATH)

