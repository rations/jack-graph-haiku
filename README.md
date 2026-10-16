# JackGraph (Haiku)

> **Users:** install the whole stack from the [JackDAW release](https://github.com/rations/jackDAW-haiku/releases)
> (`sh install.sh`). This repository is the source of the `jackgraph` package.

A native Haiku application (Interface Kit) that acts as a **JACK patchbay and server
control**: start and stop the JACK server, adjust its settings, and view and manage port
connections on a graph canvas. It is the GUI front-end of the JackDAW audio stack — see
[jackDAW-haiku/STACK.md](../jackDAW-haiku/STACK.md) for how the pieces fit together.

## Requirements

- The **`jack`** package (jack-port-haiku): `libjack` plus a `jackd` it can start/stop and
  connect to.

## Build / install

Native on Haiku (x86_64):

- **Prebuilt package:** `packaging/make-hpkg.sh` → `jackgraph-0.1-1-x86_64.hpkg`, then
  `pkgman install ./jackgraph-*.hpkg`.
- **From source:** `./build-from-source.sh` (build jack-port-haiku from source first — the
  Makefile links `-ljack` from the non-packaged tree).

A HaikuPorts recipe is in `packaging/jackgraph-0.1.recipe` (validate with
`haikuporter -c jackgraph`).

## License

MIT — see [LICENSE](LICENSE).
