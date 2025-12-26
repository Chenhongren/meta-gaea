project('gaea-i2c-tool', 'cpp',
    version : '1.0.0',
    default_options : ['cpp_std=c++23'])

executable(
    'gaea-i2c-tool',
    'Main.cpp',
    'Loopback.cpp',
    'Util.cpp',
    dependencies: [
        dependency('sdbusplus'),
        dependency(
            'boost',
            modules: ['coroutine', 'context'],
            disabler: true,
            required: false,
        ),
    ],
    install: true
)

install_data(
    'dbus-gaea_i2c_tool.conf',
    install_dir: get_option('sysconfdir') / 'dbus-1/system.d',
)

systemd_system_unit_dir = dependency('systemd').get_variable(
    'systemd_system_unit_dir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)

install_data(
    'gaea-i2c-tool.service',
    install_dir: systemd_system_unit_dir,
)
