project('sound-juicer', 'c',
  version: '3.40.0',
  meson_version: '>= 0.57.0',
)

sound_juicer_prefix = get_option('prefix')
sound_juicer_bindir = join_paths(sound_juicer_prefix, get_option('bindir'))
sound_juicer_datadir = join_paths(sound_juicer_prefix, get_option('datadir'))
sound_juicer_libdir = join_paths(sound_juicer_prefix, get_option('libdir'))
sound_juicer_libexecdir = join_paths(sound_juicer_prefix, get_option('libexecdir'))
sound_juicer_localedir = join_paths(sound_juicer_prefix, get_option('localedir'))

sound_juicer_docdir = join_paths(sound_juicer_datadir, 'doc', meson.project_name())

gnome = import('gnome')
i18n = import('i18n')

source_root = meson.current_source_dir()
po_dir = join_paths(source_root, 'po')
data_dir = join_paths(source_root, 'data')

top_inc = include_directories('.')

config_h = configuration_data()

config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_TARNAME', meson.project_name())
config_h.set_quoted('PACKAGE_URL', 'https://wiki.gnome.org/Apps/SoundJuicer')
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.gnome.org/GNOME/sound-juicer/issues/new')
config_h.set_quoted('ISO_CODES_PREFIX', dependency('iso-codes').get_pkgconfig_variable('prefix'))

c_flags = [
  '-DTOPSRCDIR="@0@"'.format(source_root),
  '-DPREFIX="@0@"'.format(sound_juicer_prefix),
  '-DDATADIR="@0@"'.format(sound_juicer_datadir),
  '-DLOCALEDIR="@0@"'.format(sound_juicer_localedir),
  '-DLIBDIR="@0@"'.format(sound_juicer_libdir),
  '-DPKGDATADIR="@0@"'.format(join_paths(sound_juicer_datadir, meson.project_name())),
  '-DHAVE_CONFIG_H',
  '-Wl,--export-dynamic'
]

if not get_option('enable-deprecations')
  c_flags += '-Wno-deprecated-declarations'
endif

add_project_arguments(c_flags, language: 'c')

deps = [
  dependency('glib-2.0', version: '>=2.49.5'),
  dependency('gthread-2.0'),
  dependency('gobject-2.0'),
  dependency('gio-2.0'),
  dependency('gtk+-3.0', version: '>=3.21.6'),
  dependency('gmodule-export-2.0'),
  dependency('libcanberra-gtk3'),
  dependency('gsettings-desktop-schemas'),
  dependency('libbrasero-media3'),
  dependency('gstreamer-1.0'),
  dependency('gstreamer-plugins-base-1.0'),
  dependency('gstreamer-pbutils-1.0'),
  dependency('libmusicbrainz5', version: '>=5.0.1'),
  dependency('iso-codes'),
  dependency('libdiscid', version: '>=0.4.0'),
]

gst_inspect = find_program('gst-inspect-1.0')
elements = ['vorbisenc', 'flacenc', 'wavenc', 'giosink']
foreach element : elements
  run_command(gst_inspect, '--exists', '--atleast-version=1.0', element, check: true)
endforeach

configure_file(
  output: 'config.h',
  configuration: config_h,
)

doc_data = files(
  'COPYING',
  'NEWS',
  'README.md',
  'AUTHORS'
)

install_data(doc_data, install_dir: sound_juicer_docdir)

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)

subdir('po')
subdir('help')
subdir('data')
subdir('libjuicer')
subdir('src')
subdir('tests')
