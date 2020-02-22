sources = [
    'sj-album-chooser-dialog.c',
    'egg-play-preview.c',
    'sj-about.c',
    'sj-cell-renderer-text.c',
    'sj-extracting.c',
    'sj-genres.c',
    'sj-main.c',
    'sj-play.c',
    'sj-prefs.c',
    'sj-tree-view.c',
    'sj-window-state.c',
]

sj_resources= gnome.compile_resources(
    'sj-resources',
    join_paths(data_dir, 'sound-juicer.gresource.xml'),
    source_dir: data_dir,
    gresource_bundle: false,
)

sources += sj_resources[0]

executable(
    meson.project_name(),
    sources,
    link_with: libjuicer,
    dependencies: deps,
    include_directories: [top_inc, '../libjuicer'],
    install: true,
)
