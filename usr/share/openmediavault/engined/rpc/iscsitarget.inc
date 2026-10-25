<?php

/**
 * @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */
class OMVRpcServiceIscsiTarget extends \OMV\Rpc\ServiceAbstract
{
    /**
     * Get the RPC service name.
     */
    public function getName()
    {
        return "ISCSITarget";
    }

    /**
     * Initialize the RPC service.
     */
    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("getLunList");
        $this->registerMethod("enumerateLuns");
        $this->registerMethod("getLun");
        $this->registerMethod("setLun");
        $this->registerMethod("deleteLun");
        $this->registerMethod("getTargetList");
        $this->registerMethod("getTarget");
        $this->registerMethod("setTarget");
        $this->registerMethod("deleteTarget");
        $this->registerMethod("getStats");
    }

    /**
     * Get the settings.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return The configuration object.
     */
    public function getSettings($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget");
        // Remove useless properties from the object.
        $object->remove("luns");
        $object->remove("targets");
        return $object->getAssoc();
    }

    /**
     * Set the settings.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return The stored configuration object.
     */
    public function setSettings($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.iscsitarget.setsettings");
        // Get the existing configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget");
        $object->setAssoc($params);
        // Set the configuration object.
        $db->set($object);
        // Remove useless properties from the object.
        $object->remove("luns");
        $object->remove("targets");
        // Return the configuration object.
        return $object->getAssoc();
    }

    /**
     * Get list of LUN configuration objects.
     * @param params An array containing the following fields:
     *   \em start The index where to start.
     *   \em limit The number of objects to process.
     *   \em sortfield The name of the column used to sort.
     *   \em sortdir The sort direction, ASC or DESC.
     * @param context The context of the caller.
     * @return An array containing the requested objects. The field \em total
     *   contains the total number of objects, \em data contains the object
     *   array. An exception will be thrown in case of an error.
     */
    public function getLunList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.getlist");
        // Get the configuration objects.
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->get("conf.service.iscsitarget.lun");
        $targets = $db->get("conf.service.iscsitarget.target");
        // Add additional LUN information.
        $objectsAssoc = [];
        foreach ($objects as $objectk => &$objectv) {
            $assoc = $objectv->getAssoc();
            $assoc['sharedfoldername'] = gettext("n/a");
            if ($db->exists("conf.system.sharedfolder", [
                "operator" => "stringEquals",
                "arg0" => "uuid",
                "arg1" => $objectv->get("sharedfolderref")
            ])) {
                $sfObject = $db->get("conf.system.sharedfolder",
                    $objectv->get("sharedfolderref"));
                $assoc['sharedfoldername'] = $sfObject->get("name");
            }
            // Collect the names of the targets exporting this LUN.
            $assoc['targetnames'] = [];
            foreach ($targets as $targetk => $targetv) {
                if (in_array($objectv->get("uuid"), explode_safe(",",
                        $targetv->get("luns")))) {
                    $assoc['targetnames'][] = $targetv->get("name");
                }
            }
            $assoc['targetnames'] = implode(",", $assoc['targetnames']);
            $objectsAssoc[] = $assoc;
        }
        // Filter the result.
        return $this->applyFilter(
            $objectsAssoc,
            $params['start'],
            $params['limit'],
            $params['sortfield'],
            $params['sortdir']
        );
    }

    /**
     * Enumerate all LUNs. Used by the target form to select the LUNs
     * to export.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return An array of objects with the fields \em uuid, \em name
     *   and \em description.
     */
    public function enumerateLuns($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        $result = [];
        $db = \OMV\Config\Database::getInstance();
        foreach ($db->get("conf.service.iscsitarget.lun") as $objectk => $objectv) {
            $result[] = [
                "uuid" => $objectv->get("uuid"),
                "name" => $objectv->get("name"),
                "description" => sprintf("%s (%d GiB)",
                    $objectv->get("name"), $objectv->get("size"))
            ];
        }
        return $result;
    }

    /**
     * Get a LUN configuration object.
     * @param params An array containing the following fields:
     *   \em uuid The UUID of the configuration object.
     * @param context The context of the caller.
     * @return The requested configuration object.
     */
    public function getLun($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        return $db->getAssoc("conf.service.iscsitarget.lun", $params['uuid']);
    }

    /**
     * Set a LUN configuration object.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return The stored configuration object.
     */
    public function setLun($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.iscsitarget.setlun");
        // Prepare the configuration object.
        $object = new \OMV\Config\ConfigObject("conf.service.iscsitarget.lun");
        $object->setAssoc($params);
        $db = \OMV\Config\Database::getInstance();
        $db->assertIsUnique($object, "name", sprintf(
            gettext("A LUN with the name '%s' already exists."),
            $object->get("name")));
        if (false === $object->isNew()) {
            // The backing file is never moved or shrunk, this would
            // destroy the data of the initiators.
            $oldObject = $db->get("conf.service.iscsitarget.lun",
                $object->get("uuid"));
            if (($oldObject->get("sharedfolderref") !==
                    $object->get("sharedfolderref")) ||
                    ($oldObject->get("filename") !== $object->get("filename"))) {
                throw new \InvalidArgumentException(gettext(
                    "The shared folder and the file name of an existing LUN can not be changed."));
            }
            if ($object->get("size") < $oldObject->get("size")) {
                throw new \InvalidArgumentException(gettext(
                    "The size of an existing LUN can not be reduced."));
            }
        } else {
            $db->assertIsUniqueByFilter($object, [
                "operator" => "and",
                "arg0" => [
                    "operator" => "stringEquals",
                    "arg0" => "sharedfolderref",
                    "arg1" => $object->get("sharedfolderref")
                ],
                "arg1" => [
                    "operator" => "stringEquals",
                    "arg0" => "filename",
                    "arg1" => $object->get("filename")
                ]
            ], gettext("Another LUN already uses this file."));
            // The path is passed to shell commands by SaltStack, so it
            // must not contain blanks or other special characters.
            $sfObject = $db->get("conf.system.sharedfolder",
                $object->get("sharedfolderref"));
            if (1 !== preg_match('/^[\w.\/-]*$/', $sfObject->get("reldirpath"))) {
                throw new \InvalidArgumentException(sprintf(gettext(
                    "The relative path of the shared folder '%s' contains blanks or special characters."),
                    $sfObject->get("name")));
            }
        }
        // Set the configuration object.
        $db->set($object);
        // Return the configuration object.
        return $object->getAssoc();
    }

    /**
     * Delete a LUN configuration object. The backing file is not removed.
     * @param params An array containing the following fields:
     *   \em uuid The UUID of the configuration object.
     * @param context The context of the caller.
     * @return The deleted configuration object.
     */
    public function deleteLun($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget.lun", $params['uuid']);
        // Refuse to delete a LUN that is still exported by a target.
        foreach ($db->get("conf.service.iscsitarget.target") as $targetk => $targetv) {
            if (in_array($object->get("uuid"), explode_safe(",",
                    $targetv->get("luns")))) {
                throw new \OMV\Exception(gettext(
                    "The LUN '%s' is still used by the target '%s'."),
                    $object->get("name"), $targetv->get("name"));
            }
        }
        $db->delete($object);
        // Return the deleted configuration object.
        return $object->getAssoc();
    }

    /**
     * Get list of target configuration objects.
     * @param params An array containing the following fields:
     *   \em start The index where to start.
     *   \em limit The number of objects to process.
     *   \em sortfield The name of the column used to sort.
     *   \em sortdir The sort direction, ASC or DESC.
     * @param context The context of the caller.
     * @return An array containing the requested objects.
     */
    public function getTargetList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.getlist");
        $db = \OMV\Config\Database::getInstance();
        $basename = $db->get("conf.service.iscsitarget")->get("basename");
        // Map the LUN UUIDs to their names.
        $lunNames = [];
        foreach ($db->get("conf.service.iscsitarget.lun") as $lunk => $lunv) {
            $lunNames[$lunv->get("uuid")] = $lunv->get("name");
        }
        $objectsAssoc = [];
        foreach ($db->get("conf.service.iscsitarget.target") as $objectk => $objectv) {
            $assoc = $objectv->getAssoc();
            // Never send the CHAP secrets to the list view.
            unset($assoc['password'], $assoc['mutualpassword']);
            $assoc['iqn'] = sprintf("%s:%s", $basename, $objectv->get("name"));
            $assoc['lunnames'] = implode(",", array_map(
                function ($uuid) use ($lunNames) {
                    return array_value($lunNames, $uuid, $uuid);
                }, explode_safe(",", $objectv->get("luns"))));
            $objectsAssoc[] = $assoc;
        }
        // Filter the result.
        return $this->applyFilter(
            $objectsAssoc,
            $params['start'],
            $params['limit'],
            $params['sortfield'],
            $params['sortdir']
        );
    }

    /**
     * Get a target configuration object.
     * @param params An array containing the following fields:
     *   \em uuid The UUID of the configuration object.
     * @param context The context of the caller.
     * @return The requested configuration object.
     */
    public function getTarget($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        $db = \OMV\Config\Database::getInstance();
        $result = $db->getAssoc("conf.service.iscsitarget.target",
            $params['uuid']);
        $result['luns'] = explode_safe(",", $result['luns']);
        return $result;
    }

    /**
     * Set a target configuration object.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return The stored configuration object.
     */
    public function setTarget($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.iscsitarget.settarget");
        $db = \OMV\Config\Database::getInstance();
        // Make sure the selected LUNs exist.
        foreach ($params['luns'] as $lunUuid) {
            $db->get("conf.service.iscsitarget.lun", $lunUuid);
        }
        $params['luns'] = implode(",", array_unique($params['luns']));
        // Normalize and validate the initiator names.
        $initiators = array_unique(array_filter(array_map("trim",
            explode_safe(",", $params['initiators'])), "strlen"));
        foreach ($initiators as $initiator) {
            if (1 !== preg_match('/^(iqn\.[0-9]{4}-[0-9]{2}\.[^\s,]+|eui\.[0-9A-Fa-f]{16}|naa\.[0-9A-Fa-f]{16,32})$/',
                    $initiator)) {
                throw new \InvalidArgumentException(sprintf(gettext(
                    "The initiator name '%s' is invalid."), $initiator));
            }
        }
        $params['initiators'] = implode(",", $initiators);
        if ("chap" === $params['authentication']) {
            // Generate the secrets if they have been left empty. The mutual
            // secret is optional, so it is only generated if a mutual user
            // name has been given.
            if (!empty($params['userid']) && empty($params['password'])) {
                $params['password'] = $this->generateSecret();
            }
            if (!empty($params['mutualuserid']) &&
                    empty($params['mutualpassword'])) {
                $params['mutualpassword'] = $this->generateSecret();
            }
            $this->assertChapCredentials($params['userid'],
                $params['password'], true);
            $this->assertChapCredentials($params['mutualuserid'],
                $params['mutualpassword'], false);
            if ($params['password'] === $params['mutualpassword']) {
                throw new \InvalidArgumentException(gettext(
                    "The mutual CHAP secret must differ from the CHAP secret."));
            }
        }
        // Prepare the configuration object.
        $object = new \OMV\Config\ConfigObject(
            "conf.service.iscsitarget.target");
        $object->setAssoc($params);
        $db->assertIsUnique($object, "name", sprintf(
            gettext("A target with the name '%s' already exists."),
            $object->get("name")));
        // Set the configuration object.
        $db->set($object);
        // Return the configuration object.
        return $object->getAssoc();
    }

    /**
     * Delete a target configuration object.
     * @param params An array containing the following fields:
     *   \em uuid The UUID of the configuration object.
     * @param context The context of the caller.
     * @return The deleted configuration object.
     */
    public function deleteTarget($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        // Delete the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget.target", $params['uuid']);
        $db->delete($object);
        // Return the deleted configuration object.
        return $object->getAssoc();
    }

    /**
     * Get the current LIO configuration and the active sessions.
     * @param params The method parameters.
     * @param context The context of the caller.
     * @return A string containing the requested statistics.
     */
    public function getStats($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        if ($this->isModuleDirty("iscsitarget")) {
            throw new \OMV\Config\ConfigDirtyException();
        }
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.iscsitarget");
        if (false === $object->get("enable")) {
            return gettext("Service disabled");
        }
        $result = [];
        foreach ([
            ["ls", "/backstores/fileio"],
            ["ls", "/iscsi"],
            ["sessions", "detail"]
        ] as $cmdArgs) {
            $cmd = new \OMV\System\Process("targetcli", $cmdArgs);
            $cmd->setEnv("PYTHONWARNINGS", "ignore");
            $cmd->execute($output);
            // Remove the dot leaders, otherwise the lines are too long
            // for the text page and get wrapped.
            $result[] = preg_replace('/ \.{2,} /', ' ',
                implode("\n", $output));
            $output = [];
        }
        return implode("\n\n", $result);
    }

    /**
     * Helper function to generate a random CHAP secret of 16 characters.
     * Look-alike characters are left out because the secret is often
     * typed in manually on the initiator side.
     * @return The generated secret.
     */
    private function generateSecret()
    {
        $chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
        $secret = "";
        for ($i = 0; $i < 16; $i++) {
            $secret .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $secret;
    }

    /**
     * Helper function to validate a CHAP user name and secret.
     * @param userid The CHAP user name.
     * @param password The CHAP secret.
     * @param required Whether the credentials are mandatory.
     * @return void
     * @throw \InvalidArgumentException
     */
    private function assertChapCredentials($userid, $password, $required)
    {
        if ((false === $required) && empty($userid) && empty($password)) {
            return;
        }
        if (empty($userid)) {
            throw new \InvalidArgumentException(gettext(
                "The CHAP user name must not be empty."));
        }
        // Most initiators, e.g. the Microsoft one, require a secret
        // of 12 to 16 characters.
        if ((strlen($password) < 12) || (strlen($password) > 16)) {
            throw new \InvalidArgumentException(gettext(
                "The CHAP secret must be 12 to 16 characters long."));
        }
    }
}
