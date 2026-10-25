<?php

/**
 * @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */
class OMVModuleIscsiTarget extends \OMV\Engine\Module\ServiceAbstract implements
    \OMV\Engine\Notify\IListener,
    \OMV\Engine\Module\IServiceStatus
{
    public function getName()
    {
        return "iscsitarget";
    }

    public function deployAfter(): array
    {
        return ["fstab"];
    }

    /**
     * Get the status of the managed service.
     */
    public function getStatus()
    {
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget");
        $systemCtl = new \OMV\System\SystemCtl("rtslib-fb-targetctl");
        return [
            "name" => $this->getName(),
            "title" => gettext("iSCSI Target"),
            "enabled" => $object->get("enable"),
            "running" => $systemCtl->isActive()
        ];
    }

    /**
     * Helper function to find out whether the given shared folder
     * configuration object is used by a LUN. If it is used, then mark
     * the module as dirty.
     * @param type The event message type.
     * @param path The event message path.
     * @param object The configuration object.
     */
    final public function onSharedFolder($type, $path, $object)
    {
        $db = \OMV\Config\Database::getInstance();
        if (true === $db->exists("conf.service.iscsitarget.lun", [
            "operator" => "stringEquals",
            "arg0" => "sharedfolderref",
            "arg1" => $object['uuid']
        ])) {
            $this->setDirty();
        }
    }

    public function bindListeners(\OMV\Engine\Notify\Dispatcher $dispatcher)
    {
        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.conf.service.iscsitarget",
            [$this, "setDirty"]
        );
        $dispatcher->addListener(
            OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY | OMV_NOTIFY_DELETE,
            "org.openmediavault.conf.service.iscsitarget.lun",
            [$this, "setDirty"]
        );
        $dispatcher->addListener(
            OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY | OMV_NOTIFY_DELETE,
            "org.openmediavault.conf.service.iscsitarget.target",
            [$this, "setDirty"]
        );
        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.conf.system.sharedfolder",
            [$this, "onSharedFolder"]
        );
    }
}
