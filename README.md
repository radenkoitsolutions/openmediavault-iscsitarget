# openmediavault-iscsitarget

An iSCSI target plugin for **openmediavault 8**. It exports file-backed LUNs over iSCSI using the in-kernel **LIO** target and is managed entirely from the OMV web interface (**Services → iSCSI Target**).

The old `openmediavault-iscsitarget` plugin relied on the unmaintained IET kernel module and was dropped from OMV. This plugin replaces it with the LIO target (`targetcli-fb` / `python3-rtslib-fb`), which ships with the Debian kernel.

> **Status:** developed and tested on openmediavault 8.3.1 (Debian 13 "trixie", kernel 6.12). Tested with the Linux `open-iscsi` initiator. Treat it as young software and keep backups of data you export.

## Features

- File-backed LUNs (LIO `fileio` backstore) stored in an OMV **shared folder**
  - backing file is created on **Apply** with `fallocate` (space is reserved up front), mode `0600`
  - `chattr +C` (no copy-on-write) is set on new files, which matters on **btrfs**
  - LUN size can be **increased** later from the UI, never reduced
  - optional **write-back cache** and **thin provisioning (UNMAP/TRIM)** per LUN
- Multiple targets, each exporting one or more LUNs
- **Initiator access control**: list the allowed initiator IQNs (one ACL per initiator), or leave the list empty to allow any initiator
- **CHAP** authentication, including optional **mutual CHAP**
  - leave the secret empty and a random 16-character secret is generated when you save
  - secrets can be viewed and copied from the target form
- Configurable IQN base name, listen address and port
- **Status** page with the live LIO configuration and active sessions
- Integrates with OMV's normal workflow: changes are marked as pending and applied with the yellow *Pending configuration changes* banner
- The service survives reboots, including setups where the data disk is mounted late (see [How it works](#how-it-works))

## Requirements

- openmediavault **8.0 or newer** (Debian 13)
- A file system with a **shared folder** to hold the LUN files (ext4, XFS and btrfs work; on btrfs use a directory with copy-on-write disabled, which the plugin does for the files it creates)
- Packages pulled in automatically: `targetcli-fb`, `python3-rtslib-fb`

## Installation

### From a release / built package

```sh
scp openmediavault-iscsitarget_*_all.deb root@nas:/root/
ssh root@nas apt-get install -y /root/openmediavault-iscsitarget_*_all.deb
```

Reload the web interface afterwards (the package restarts the engine and rebuilds the Workbench configuration, but an open browser tab keeps the old menu until it is reloaded).

### Building the package

Requires Docker. The package is built in a `debian:trixie` container, so no Debian tooling is needed on the host:

```sh
./build.sh
# -> dist/openmediavault-iscsitarget_<version>_all.deb
```

### Removing

```sh
apt-get remove openmediavault-iscsitarget   # keeps the OMV configuration
apt-get purge  openmediavault-iscsitarget   # also deletes the plugin configuration
```

Removing the package stops the LIO target. **LUN backing files are never deleted** by the plugin.

## Usage

Open **Services → iSCSI Target**. Work through the pages in this order.

### 1. Settings

| Field | Meaning |
|---|---|
| Enabled | Starts or stops the target service. |
| Base name | IQN prefix of all targets, format `iqn.yyyy-mm.reversed.domain`, e.g. `iqn.2026-10.lan.example`. A target's full IQN is `<base name>:<target name>`. **Changing it changes the IQN of every target**, so initiators must be reconfigured. |
| Listen address | IPv4 address of the portal. `0.0.0.0` listens on all interfaces. |
| Port | TCP port, default `3260`. |

The default base name is derived from the host name and the current month when the plugin is first installed.

### 2. LUNs

| Field | Meaning |
|---|---|
| Name | Name of the backstore (letters, digits, `-`, `_`). |
| Shared folder | Where the backing file is stored. **Cannot be changed later.** |
| File name | Backing file inside the shared folder, e.g. `disk0.img`. Created on Apply if it does not exist. **Cannot be changed later.** |
| Size (GiB) | Size of the LUN. Can only grow. |
| Write-back cache | Faster, but acknowledged writes can be lost on power failure. Off by default (write-through). |
| Thin provisioning | Lets initiators discard blocks (UNMAP/TRIM). |

Deleting a LUN removes it from the configuration only. The backing file stays on disk and must be removed by hand. A LUN that is still exported by a target cannot be deleted.

### 3. Targets

| Field | Meaning |
|---|---|
| Name | Appended to the base name: name `proxmox` becomes `<base name>:proxmox`. Lower case letters, digits, `.`, `-`, `:`. |
| LUNs | LUNs exported by the target. They are numbered 0, 1, 2 … in list order. Adding or removing a LUN can renumber the others. |
| Allowed initiators | IQNs allowed to log in. Empty means **any initiator can log in**. |
| Authentication | `None` or `CHAP`. |
| User name / Secret | CHAP credentials. The secret must be 12–16 characters; leave it empty to generate one. |
| Mutual user name / secret | Optional, lets the initiator authenticate the target. The mutual secret must differ from the CHAP secret. |

When initiators are listed, the plugin creates one ACL per initiator and the CHAP credentials are applied per ACL. With no initiators, the credentials apply to the portal group.

After saving, click the yellow banner to **apply** the changes. Applying restarts the LIO service (see [Limitations](#limitations)).

### 4. Status

Shows `targetcli ls` for the file backstores and iSCSI targets, and `targetcli sessions detail`.

## Example: shared storage for Proxmox VE

1. Create a LUN, e.g. name `proxmox0`, shared folder `iscsi`, file `proxmox0.img`, 100 GiB.
2. Create a target named `proxmox`, select the LUN, and enter the IQN of every Proxmox node under *Allowed initiators*. Each node's IQN is in `/etc/iscsi/initiatorname.iscsi`.
3. Optionally enable CHAP and leave the secret empty to generate it.
4. Apply the changes and enable the service in **Settings**.
5. In Proxmox: **Datacenter → Storage → Add → iSCSI**, enter the NAS address and the target IQN, and untick *Use LUNs directly*.
6. If you use CHAP, the Proxmox GUI cannot store the credentials. Set them on every node with `iscsiadm`, then log in again:

   ```sh
   T=iqn.2026-10.lan.example:proxmox
   P=192.168.1.10
   iscsiadm -m node -T $T -p $P -o update -n node.session.auth.authmethod -v CHAP
   iscsiadm -m node -T $T -p $P -o update -n node.session.auth.username   -v <user>
   iscsiadm -m node -T $T -p $P -o update -n node.session.auth.password   -v <secret>
   iscsiadm -m node -T $T -p $P --login
   ```
7. **Datacenter → Storage → Add → LVM** on top of the iSCSI storage, and tick **Shared** to use it from all nodes.

## How it works

| Part | File |
|---|---|
| Configuration model (`/config/services/iscsitarget` in `config.xml`) | `usr/share/openmediavault/datamodels/conf.service.iscsitarget*.json` |
| Default configuration created on install | `usr/share/openmediavault/confdb/create.d/conf.service.iscsitarget.sh` |
| RPC service `ISCSITarget` (validation, CRUD, status) | `usr/share/openmediavault/engined/rpc/iscsitarget.inc` |
| Engine module (dirty tracking, service status) | `usr/share/openmediavault/engined/module/iscsitarget.inc` |
| Deployment (SaltStack) | `srv/salt/omv/deploy/iscsitarget/` |
| Web interface (YAML) | `usr/share/openmediavault/workbench/{navigation,route,component}.d/` |
| Packaging | `debian/`, `build.sh` |

**Deployment.** On Apply, the Salt state `omv.deploy.iscsitarget`:

1. creates missing backing files (`mkdir -p`, `touch`, `chmod 0600`, `chattr +C`, `fallocate`) and grows files whose configured size increased,
2. renders the complete LIO configuration to `/etc/rtslib-fb-target/saveconfig.json` (mode `0600`),
3. writes a systemd drop-in, and
4. restarts `rtslib-fb-targetctl.service`, which clears the running LIO configuration and restores it from the file.

The generated file is the single source of truth. **Changes made with `targetcli` are overwritten on the next Apply.**

**Late-mounted data disks.** OMV mounts data file systems with `nofail`, so `local-fs.target` does not wait for them. Without help, `rtslib-fb-targetctl` could start before the LUN files are reachable and silently come up empty. The plugin therefore writes `/etc/systemd/system/rtslib-fb-targetctl.service.d/openmediavault.conf` with `RequiresMountsFor=` listing the mount point of every file system that holds a LUN.

**Secrets.** CHAP secrets are stored in clear text in OMV's `config.xml` (readable by root and the `openmediavault-config` group) and in `saveconfig.json` (root only). This is inherent to CHAP, but keep it in mind for backups of `config.xml`.

## Limitations

- **Applying restarts the target service.** Active sessions are dropped and initiators reconnect. Most initiators recover on their own, but running I/O pauses. Apply during a quiet moment.
- Only the `fileio` backstore is supported (no raw block devices, no `block` backstore, no LVM volumes).
- One portal (one IPv4 address and port) for all targets.
- One portal group (`tpg1`) per target. No multipath-specific (ALUA) tuning.
- No discovery (SendTargets) authentication.
- The shared folder and file name of a LUN cannot be changed, and the size cannot be reduced, to protect data.
- Deleting a LUN or target does not delete backing files.
- The relative path of a shared folder holding a LUN must not contain blanks or special characters (it is passed to shell commands by Salt).
- The plugin has no translations yet (English only; OMV's own text is translated as usual).

## Troubleshooting

```sh
omv-salt deploy run iscsitarget        # apply by hand and see errors
targetcli ls                           # live LIO configuration
targetcli sessions detail              # who is connected
systemctl status rtslib-fb-targetctl   # restore service
journalctl -u rtslib-fb-targetctl -b   # restore errors after boot
ss -ltn | grep 3260                    # is the portal listening?
```

| Symptom | Likely cause |
|---|---|
| Menu entry is missing after install | Reload the browser tab. If it is still missing, run `omv-mkworkbench all` and reload again. |
| Initiator sees the target but login fails | Wrong CHAP credentials, or the initiator IQN is not in *Allowed initiators*. |
| Target list is empty after a reboot | The data file system was not mounted when the service started. Check `systemctl status rtslib-fb-targetctl` and that the drop-in `openmediavault.conf` exists. |
| Apply fails with a Jinja or Salt error | Run `omv-salt deploy run iscsitarget` and read the first error. |
| New LUN is much smaller on disk than configured | The file system does not support `fallocate`; the file is sparse. |

## Development

```sh
./build.sh                                   # build the .deb
scp dist/*.deb root@nas:/root/
ssh root@nas apt-get install -y --reinstall /root/openmediavault-iscsitarget_*_all.deb
```

For a quick edit loop on a test machine, copy individual files to their install locations (`/usr/share/openmediavault/...`, `/srv/salt/omv/deploy/iscsitarget/`) and run `omv-salt deploy run iscsitarget`. After changing Workbench YAML run `omv-mkworkbench all` (reinstalling the package triggers the same rebuild) and reload the browser tab.

A convenient way to test without a second machine is the Linux initiator on the NAS itself:

```sh
apt-get install open-iscsi
iscsiadm -m discovery -t sendtargets -p 127.0.0.1
iscsiadm -m node -T <iqn> -p 127.0.0.1 --login
```

Set the initiator IQN in `/etc/iscsi/initiatorname.iscsi` to one listed in the target's *Allowed initiators*. Remove `open-iscsi` again when done.

The OMV plugin conventions (datamodels, RPC, Salt deploy, Workbench YAML) follow the official plugins such as `openmediavault-tftp` and `openmediavault-usbbackup`, which are good references.

## License

GPL-3.0-or-later, the same license as openmediavault. See `debian/copyright`.
