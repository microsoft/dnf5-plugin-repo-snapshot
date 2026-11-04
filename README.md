# DNF5 repository snapshot plugin

Use `--snapshot-time` to limit DNF5 repository packages to those published at
or before a chosen time.

## Usage

The cutoff is a Unix timestamp. For example, to use packages published by
November 1, 2026 at midnight UTC:

```sh
cutoff=$(date -u -d '2026-11-01 00:00:00' +%s)

dnf5 --snapshot-time="$cutoff" repoquery --available bash
dnf5 --snapshot-time="$cutoff" install bash
dnf5 --snapshot-time="$cutoff" upgrade
```

To leave selected repositories unrestricted, use their IDs or glob patterns:

```sh
dnf5 --snapshot-time="$cutoff" --snapshot-exclude-repos='hotfix*,extra' install bash
```

The option supports several forms:

| Selection | Example |
|---|---|
| One repository | `--snapshot-exclude-repos=hotfix` |
| Several repositories | `--snapshot-exclude-repos=hotfix,extra` |
| Glob pattern | `--snapshot-exclude-repos='hotfix*'` |
| Repeated option | `--snapshot-exclude-repos=hotfix --snapshot-exclude-repos=extra` |

## Build and install

Requirements:

- Linux, a C++20 compiler, CMake 3.18+, Make, and pkg-config.
- Development headers and libraries for DNF5, libdnf5, libdnf5-cli, and libsolv.
- An SDK built with the [Azure Linux API patches](https://github.com/microsoft/azurelinux/pull/19832)
  for publication timestamps and plugin hooks.

```sh
cmake -S . -B .build -DSNAPSHOT_BUILD_PLUGINS=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_SYSCONFDIR=/etc
cmake --build .build
ctest --test-dir .build --output-on-failure
sudo cmake --install .build
```

Match the install prefix and `CMAKE_INSTALL_LIBDIR` to your DNF5 installation.
Set `PKG_CONFIG_PATH` if your SDK is outside the usual search paths.

For unit tests without the SDK, configure with `-DSNAPSHOT_BUILD_PLUGINS=OFF`
and skip installation.

## How it works

After DNF5 loads repository metadata, the backend excludes available packages
whose rpm-md `<time file>` publication timestamp is newer than the cutoff.
Installed packages and cached metadata are unchanged. DNF5 resolves dependencies
normally; this filter does not recreate a historical repository or guarantee
that its remaining packages have all their dependencies.

With a cutoff, missing publication timestamps and direct local/URL RPM inputs
fail explicitly.

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
