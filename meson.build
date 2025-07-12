project(
	'minimize_test',
	'c',
	version: '0.0.1',
	license: 'MIT',
	meson_version: '>=0.50.0',
	default_options: [
		'warning_level=2',
		'werror=false',
	],
)


gtk = dependency('gtk+-3.0')
gdk = dependency('gdk-3.0')

wayland_client = dependency('wayland-client')
wayland_scanner = find_program('wayland-scanner')

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protocols = [
    './wlr-foreign-toplevel-management-unstable-v1.xml'
]

protos_client_src = []
protos_headers = []

foreach p : protocols
	xml = join_paths(p)
	protos_headers += wayland_scanner_client.process(xml)
	protos_client_src += wayland_scanner_code.process(xml)
endforeach

lib_protos = static_library('grabber_protos', protos_client_src + protos_headers,
	dependencies: [wayland_client]) # for the include directory

lib_protos_dep = declare_dependency(
	link_with: lib_protos,
	sources: protos_headers,
)



sources = ['minimize_test.c']

mt = executable('mt', sources, dependencies: [gtk, gdk, lib_protos_dep])


