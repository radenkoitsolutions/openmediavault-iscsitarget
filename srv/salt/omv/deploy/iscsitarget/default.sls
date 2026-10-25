# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

# The LIO kernel target is configured by rendering the rtslib-fb
# configuration file and restarting the 'rtslib-fb-targetctl' service,
# which clears the running configuration and restores it from that file.
# Changes made with 'targetcli' are overwritten on the next deployment.

# Testing:
# targetcli ls
# targetcli sessions detail
# iscsiadm -m discovery -t sendtargets -p <nas>

{% set config = salt['omv_conf.get']('conf.service.iscsitarget') %}
{% set luns = salt['omv_conf.get']('conf.service.iscsitarget.lun') %}
{% set targets = salt['omv_conf.get']('conf.service.iscsitarget.target') %}
{% set saveconfig_file = '/etc/rtslib-fb-target/saveconfig.json' %}
{% set dropin_file = '/etc/systemd/system/rtslib-fb-targetctl.service.d/openmediavault.conf' %}

{% if config.enable | to_bool %}

{% set storage_objects = [] %}
{% set backstores = {} %}
{% set mount_dirs = [] %}

{% for lun in luns %}
{% set lun_dir = salt['omv_conf.get_sharedfolder_path'](lun.sharedfolderref) %}
{% set lun_path = lun_dir | path_join(lun.filename) %}
{% set lun_size = (lun.size | int) * 1073741824 %}
{% set writeback = lun.writeback | to_bool %}

# New backing files are only accessible by root, the shared folder may be
# readable by other users. They get the No_COW attribute before any data is
# written, otherwise btrfs fragments them heavily. The 'chattr' call fails
# silently on file systems that do not support it.
create_iscsitarget_lun_{{ lun.uuid }}:
  cmd.run:
    - name: >-
        mkdir -p '{{ lun_dir }}' &&
        touch '{{ lun_path }}' &&
        chmod 0600 '{{ lun_path }}' &&
        { chattr +C '{{ lun_path }}' 2>/dev/null || true; } &&
        fallocate -l {{ lun_size }} '{{ lun_path }}'
    - unless: test -e '{{ lun_path }}'

# Grow the backing file if the LUN size has been increased. The file is
# never shrunk.
grow_iscsitarget_lun_{{ lun.uuid }}:
  cmd.run:
    - name: fallocate -l {{ lun_size }} '{{ lun_path }}'
    - onlyif: test $(stat -c %s '{{ lun_path }}') -lt {{ lun_size }}
    - require:
      - cmd: create_iscsitarget_lun_{{ lun.uuid }}
    - require_in:
      - service: start_iscsitarget_service

{% do mount_dirs.append(salt['omv_conf.get_sharedfolder_mount_path'](lun.sharedfolderref).rstrip('/')) %}
{% do backstores.update({lun.uuid: lun.name}) %}
{% do storage_objects.append({
  'plugin': 'fileio',
  'name': lun.name,
  'dev': lun_path,
  'size': lun_size,
  'write_back': writeback,
  'wwn': lun.uuid,
  'attributes': {
    'emulate_tpu': 1 if lun.unmap | to_bool else 0,
    'emulate_write_cache': 1 if writeback else 0
  }
}) %}
{% endfor %}

{% set lio_targets = [] %}
{% for target in targets %}
{% set chap = target.authentication == 'chap' %}
{% set initiators = target.initiators.split(',') | map('trim') | reject('equalto', '') | list %}
{% set tpg_luns = [] %}
{% for lun_uuid in target.luns.split(',') | reject('equalto', '') %}
{% if lun_uuid in backstores %}
{% do tpg_luns.append({
  'index': tpg_luns | length,
  'storage_object': '/backstores/fileio/' ~ backstores[lun_uuid]
}) %}
{% endif %}
{% endfor %}
{% set chap_settings = {} %}
{% if chap %}
{% do chap_settings.update({
  'chap_userid': target.userid,
  'chap_password': target.password
}) %}
{% if target.mutualuserid | length > 0 %}
{% do chap_settings.update({
  'chap_mutual_userid': target.mutualuserid,
  'chap_mutual_password': target.mutualpassword
}) %}
{% endif %}
{% endif %}
{% set node_acls = [] %}
{% for initiator in initiators %}
{% set node_acl = {'node_wwn': initiator} %}
{% set mapped_luns = [] %}
{% for tpg_lun in tpg_luns %}
{% do mapped_luns.append({
  'index': tpg_lun.index,
  'tpg_lun': tpg_lun.index,
  'write_protect': False
}) %}
{% endfor %}
{% do node_acl.update({'mapped_luns': mapped_luns}) %}
{% do node_acl.update(chap_settings) %}
{% do node_acls.append(node_acl) %}
{% endfor %}
# Without initiator names every initiator may log in (demo mode).
{% set tpg = {
  'tag': 1,
  'enable': target.enable | to_bool,
  'attributes': {
    'authentication': 1 if chap else 0,
    'generate_node_acls': 0 if initiators else 1,
    'cache_dynamic_acls': 0 if initiators else 1,
    'demo_mode_write_protect': 0
  },
  'parameters': {
    'AuthMethod': 'CHAP' if chap else 'None'
  },
  'luns': tpg_luns,
  'node_acls': node_acls,
  'portals': [{
    'ip_address': config.address,
    'port': config.port | int
  }]
} %}
{% do tpg.update(chap_settings) %}
{% do lio_targets.append({
  'fabric': 'iscsi',
  'wwn': config.basename ~ ':' ~ target.name,
  'tpgs': [tpg]
}) %}
{% endfor %}

configure_iscsitarget_saveconfig:
  file.serialize:
    - name: "{{ saveconfig_file }}"
    - dataset: {{ {'fabric_modules': [], 'storage_objects': storage_objects, 'targets': lio_targets} | json }}
    - serializer: json
    - makedirs: True
    - user: root
    - group: root
    - mode: '0600'

# The data file systems are mounted with 'nofail', so 'local-fs.target'
# does not wait for them. Make sure the configuration is restored after
# the backing files are available.
{% if mount_dirs | length > 0 %}
configure_iscsitarget_mount_dependency:
  file.managed:
    - name: "{{ dropin_file }}"
    - makedirs: True
    - contents: |
        {{ pillar['headers']['auto_generated'] }}
        {{ pillar['headers']['warning'] }}
        [Unit]
        RequiresMountsFor={{ mount_dirs | unique | join(' ') }}
    - user: root
    - group: root
    - mode: '0644'
{% else %}
configure_iscsitarget_mount_dependency:
  file.absent:
    - name: "{{ dropin_file }}"
{% endif %}

iscsitarget_systemctl_daemon_reload:
  module.run:
    - service.systemctl_reload:
    - onchanges:
      - file: configure_iscsitarget_mount_dependency

start_iscsitarget_service:
  service.running:
    - name: rtslib-fb-targetctl
    - enable: True
    - require:
      - module: iscsitarget_systemctl_daemon_reload
    - watch:
      - file: configure_iscsitarget_saveconfig

{% else %}

# Stopping the service clears the running LIO configuration.
stop_iscsitarget_service:
  service.dead:
    - name: rtslib-fb-targetctl
    - enable: False

remove_iscsitarget_mount_dependency:
  file.absent:
    - name: "{{ dropin_file }}"

{% endif %}
