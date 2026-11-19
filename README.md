# DNF5 repository snapshot plugin

Limit DNF5 repository packages to those published at or before a configured
time. The plugin is a libdnf5 module; it adds no DNF5 command-line options.

## Configuration

The installed `/etc/dnf/libdnf5-plugins/repo_snapshot.conf` contains:

```ini
[main]
name = repo_snapshot
enabled = 1
snapshot-time = ${repo_snapshot_time:-}
snapshot-exclude-repos = ${repo_snapshot_exclude_repos:-}
```

`snapshot-time` is a nonnegative decimal Unix timestamp in the full unsigned
64-bit range. **Unset or empty** means no snapshot filtering; **`0` is an
active cutoff**. `snapshot-exclude-repos` is an optional comma- or
whitespace-separated list of enabled repository ID globs whose entire
repositories bypass the cutoff. Nonempty exemptions require a cutoff; invalid
values, unmatched globs, and missing in-scope publication timestamps cause
errors rather than silently reverting to live repositories.

The shipped configuration references native DNF variables with an explicit
empty default. To use packages published by November 1, 2026 at midnight UTC:

```sh
cutoff=$(date -u -d '2026-11-01 00:00:00' +%s)
dnf5 --setvar="repo_snapshot_time=$cutoff" repoquery --available bash
dnf5 --setvar="repo_snapshot_time=$cutoff" install bash
dnf5 --setvar="repo_snapshot_time=$cutoff" upgrade
```

For an exemption on a single invocation:

```sh
dnf5 --setvar="repo_snapshot_time=$cutoff" \
  --setvar='repo_snapshot_exclude_repos=hotfix*,extra' install bash
```

To persist values in DNF's vars directory, use `dnf5 config-manager setvar
repo_snapshot_time="$cutoff"` and `dnf5 config-manager setvar
repo_snapshot_exclude_repos='hotfix*,extra'`. Use `dnf5 config-manager unsetvar
repo_snapshot_time repo_snapshot_exclude_repos` to remove persisted values.
Native DNF variable precedence applies: `--setvar` overrides varsdir values
for one invocation. To suspend a persisted cutoff temporarily, pass
`--setvar=repo_snapshot_time=` (and clear the exemption variable too if it is
nonempty); `--setvar=repo_snapshot_time=0` instead filters out every package
with a publication timestamp later than zero.

Alternatively, edit the config to use literal values such as
`snapshot-time = 1793491200` and `snapshot-exclude-repos = hotfix*,extra`.
Literal values do **not** consult the DNF variables: to restore variable
control, restore the `${...:-}` references above. Disable the native plugin
with `enabled = 0`, `--disable-plugin=repo_snapshot`, or DNF5's global
`--no-plugins` option; a disabled plugin does not validate its snapshot
settings or filter packages.

## Build and install

Requirements:

- Linux, a C++20 compiler, CMake 3.18+, Make, and pkg-config.
- Development headers and libraries for libdnf5 (and its dependencies).
- A libdnf5 SDK with the [Azure Linux publication-timestamp accessor patch](https://github.com/microsoft/azurelinux/pull/19832)
  providing `Package::get_file_time()`; a stock SDK does not have it.

```sh
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_SYSCONFDIR=/etc
cmake --build .build
ctest --test-dir .build --output-on-failure
sudo cmake --install .build
```

Match the install prefix and `CMAKE_INSTALL_LIBDIR` to your DNF5 installation.
Only `repo_snapshot.so` and `repo_snapshot.conf` are installed, under
`libdnf5/plugins` and `dnf/libdnf5-plugins` respectively. Set
`PKG_CONFIG_PATH` if your SDK is outside the usual search paths. Packaging
must depend on the matching patched libdnf5 runtime.

The plugin is always built. Core and configuration tests are enabled by default;
use `-DBUILD_TESTING=OFF` to build without tests. CI currently checks formatting
only; building and testing require the patched SDK.

## How it works

After DNF5 loads variables during Base setup, the plugin resolves its config
using native libdnf5 variable substitution. After repository metadata loads,
it adds exclusions for available packages whose rpm-md `<time file>`
publication timestamp is newer than the cutoff; packages published exactly
at the cutoff remain eligible. Installed packages and cached metadata are
unchanged. Existing user exclusions are preserved. DNF5's normal repository
skip, download, and dependency-solving behavior still applies; this filter
does not recreate a historical repository or guarantee that its remaining
packages have all their dependencies.

With an active cutoff, missing publication timestamps for packages in
nonexempt enabled repositories and direct local/URL RPM inputs fail explicitly.

## Further reading

- [DNF5 documentation](https://dnf5.readthedocs.io/en/latest/)
- [DNF5 plugin development](https://dnf5.readthedocs.io/en/latest/tutorial/plugins/index.html)
- [Security policy](SECURITY.md)
- [MIT license](LICENSE)

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit [Contributor License Agreements](https://cla.opensource.microsoft.com).

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
